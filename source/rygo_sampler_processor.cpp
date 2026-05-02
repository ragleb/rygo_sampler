#include "rygo_sampler_processor.h"
#include "rygo_sampler_ids.h"
#include "base/source/fdebug.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#if SMTG_OS_MACOS || SMTG_OS_LINUX
#include <dlfcn.h>
#endif

namespace Steinberg::Vst::RygoSampler {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = kPi * 2.0;
constexpr double kFallbackDurationSeconds = 0.8;
constexpr double kFallbackFadeFraction = 0.05;

#if DEVELOPMENT
#define RYGO_LOG(...) FDebugPrint (__VA_ARGS__)
#else
#define RYGO_LOG(...)
#endif

std::string makeSampleFileName (int32 sampleNumber)
{
	std::ostringstream os;
	os << "Rygo_" << std::setw (3) << std::setfill ('0') << sampleNumber << ".wav";
	return os.str ();
}

std::string joinPath (const std::string& base, const std::string& suffix)
{
	if (base.empty ())
		return suffix;
	if (!base.empty () && base.back () == '/')
		return base + suffix;
	return base + '/' + suffix;
}

std::string parentPath (const std::string& path)
{
	const auto pos = path.find_last_of ("/\\");
	if (pos == std::string::npos)
		return {};
	return path.substr (0, pos);
}

std::string stripTrailingSlash (std::string value)
{
	while (!value.empty () && (value.back () == '/' || value.back () == '\\'))
	{
		value.pop_back ();
	}
	return value;
}

std::vector<std::string> buildSampleSearchPaths ()
{
	std::vector<std::string> paths;
	auto addPath = [&paths] (const std::string& candidate) {
		if (candidate.empty ())
			return;
		const auto normalized = stripTrailingSlash (candidate);
		if (normalized.empty ())
			return;
		if (std::find (paths.begin (), paths.end (), normalized) == paths.end ())
			paths.emplace_back (normalized);
	};

	if (const char* envPath = std::getenv ("RYGO_SAMPLES_PATH"))
		addPath (envPath);

#if SMTG_OS_MACOS || SMTG_OS_LINUX
	Dl_info info {};
	if (dladdr (reinterpret_cast<void*> (&Processor::createInstance), &info) != 0 &&
	    info.dli_fname != nullptr)
	{
		std::string modulePath (info.dli_fname);
		const auto moduleDir = parentPath (modulePath);
		const auto contentsDir = parentPath (moduleDir);
		if (!contentsDir.empty ())
		{
			addPath (joinPath (contentsDir, "Resources/Samples"));
			addPath (joinPath (contentsDir, "Resources/rgsamples"));
		}
	}
#endif

	if (const char* home = std::getenv ("HOME"))
	{
		std::string homePath (home);
		addPath (joinPath (homePath, "Documents/Rygo/Samples"));
		addPath (joinPath (homePath, "Music/Rygo/Samples"));
		addPath (joinPath (homePath, "Library/Application Support/Rygo/Samples"));
		addPath (joinPath (homePath, "Documents/ZHRI/Samples"));
		addPath (joinPath (homePath, "Music/ZHRI/Samples"));
		addPath (joinPath (homePath, "Library/Application Support/ZHRI/Samples"));
	}

#if SMTG_OS_WINDOWS
	// Windows uses USERPROFILE instead of HOME
	if (const char* userProfile = std::getenv ("USERPROFILE"))
	{
		std::string up (userProfile);
		addPath (joinPath (up, "Documents\\Rygo\\Samples"));
		addPath (joinPath (up, "Documents/Rygo/Samples"));
		addPath (joinPath (up, "Music\\Rygo\\Samples"));
	}
	// Also check common program data locations
	if (const char* appData = std::getenv ("APPDATA"))
	{
		addPath (joinPath (std::string (appData), "Rygo\\Samples"));
	}
	addPath ("C:\\ProgramData\\Rygo\\Samples");
#endif

	addPath ("/Library/Application Support/Rygo/Samples");
	addPath ("/Library/Application Support/ZHRI/Samples");

	return paths;
}
}

const FUID ProcessorUID (0xB6F510D1, 0x0C7B4FD9, 0x8749E2F2, 0x290D69AF);
const FUID ControllerUID (0x932D2E93, 0x6E1348E0, 0xB2DE6C94, 0xCBA3AE31);

//------------------------------------------------------------------------
// BiquadFilter
//------------------------------------------------------------------------
void BiquadFilter::setup (double sr)
{
	sampleRate = std::max (sr, 1.0);
	reset ();
}

void BiquadFilter::setLowPass (double cutoffHz, double q)
{
	cutoffHz = std::clamp (cutoffHz, 20.0, sampleRate * 0.45);
	q = std::max (q, 0.1);

	const double omega = kTwoPi * cutoffHz / sampleRate;
	const double sinw = std::sin (omega);
	const double cosw = std::cos (omega);
	const double alpha = sinw / (2.0 * q);

	const double b0Raw = (1.0 - cosw) * 0.5;
	const double b1Raw = 1.0 - cosw;
	const double b2Raw = (1.0 - cosw) * 0.5;
	const double a0Raw = 1.0 + alpha;
	const double a1Raw = -2.0 * cosw;
	const double a2Raw = 1.0 - alpha;

	const double invA0 = 1.0 / a0Raw;
	b0 = b0Raw * invA0;
	b1 = b1Raw * invA0;
	b2 = b2Raw * invA0;
	a1 = a1Raw * invA0;
	a2 = a2Raw * invA0;
}

void BiquadFilter::reset ()
{
	z1 = 0.0;
	z2 = 0.0;
}

double BiquadFilter::process (double input)
{
	const double out = b0 * input + z1;
	z1 = b1 * input - a1 * out + z2;
	z2 = b2 * input - a2 * out;
	return out;
}

//------------------------------------------------------------------------
// SimpleReverb::DelayLine
//------------------------------------------------------------------------
void SimpleReverb::DelayLine::resize (size_t newSize)
{
	if (newSize == 0)
		newSize = 1;
	buffer.assign (newSize, 0.0);
	index = 0;
}

double SimpleReverb::DelayLine::process (double input)
{
	if (buffer.empty ())
		return input;
	const double delayed = buffer[index];
	buffer[index] = input + delayed * feedback;
	index++;
	if (index >= buffer.size ())
		index = 0;
	return delayed;
}

void SimpleReverb::DelayLine::reset ()
{
	std::fill (buffer.begin (), buffer.end (), 0.0);
	index = 0;
}

//------------------------------------------------------------------------
// SimpleReverb
//------------------------------------------------------------------------
void SimpleReverb::setup (double sr)
{
	sampleRate = std::max (sr, 1.0);
	updateDelayLengths ();
	reset ();
}

void SimpleReverb::setParameters (double sizeNormalized, double wetNormalized)
{
	size = std::clamp (sizeNormalized, 0.0, 1.0);
	wet = std::clamp (wetNormalized, 0.0, 1.0);
	updateDelayLengths ();
}

void SimpleReverb::reset ()
{
	for (auto& delay : delaysL)
		delay.reset ();
	for (auto& delay : delaysR)
		delay.reset ();
}

void SimpleReverb::updateDelayLengths ()
{
	const double baseSeconds = 0.05 + size * 0.35;
	const std::array<double, kDelayCount> ratios {1.0, 1.33, 1.61, 1.87};
	for (int i = 0; i < kDelayCount; ++i)
	{
		const size_t delaySamples =
		    static_cast<size_t> (std::max (1.0, baseSeconds * ratios[i] * sampleRate));
		delaysL[i].resize (delaySamples);
		delaysR[i].resize (delaySamples);
		delaysL[i].feedback = 0.55 + size * 0.35;
		delaysR[i].feedback = 0.55 + size * 0.35;
	}
}

void SimpleReverb::process (double inL, double inR, double& outL, double& outR)
{
	double wetAccumL = 0.0;
	double wetAccumR = 0.0;
	double inputL = inL;
	double inputR = inR;

	for (int i = 0; i < kDelayCount; ++i)
	{
		const double delayedL = delaysL[i].process (inputL);
		const double delayedR = delaysR[i].process (inputR);
		// light cross feed for stereo width
		inputL = inL * 0.6 + delayedR * 0.4;
		inputR = inR * 0.6 + delayedL * 0.4;
		wetAccumL += delayedL;
		wetAccumR += delayedR;
	}

	const double dryMix = 1.0 - wet;
	const double wetMix = wet / kDelayCount;
	outL = dryMix * inL + wetMix * wetAccumL;
	outR = dryMix * inR + wetMix * wetAccumR;
}

//------------------------------------------------------------------------
// Processor
//------------------------------------------------------------------------
Processor::Processor ()
{
	setControllerClass (ControllerUID);
}

FUnknown* Processor::createInstance (void*)
{
	return (IAudioProcessor*)new Processor ();
}

tresult PLUGIN_API Processor::initialize (FUnknown* context)
{
	auto result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioOutput (USTRING ("Stereo Out"), SpeakerArr::kStereo);
	addEventInput (USTRING ("Event In"), 1);

	updateNoteMapping ();

	return kResultOk;
}

tresult PLUGIN_API Processor::setActive (TBool state)
{
	if (state)
	{
		resetVoices ();
		reverb.reset ();
		filterL.reset ();
		filterR.reset ();
	}
	else
	{
		resetVoices ();
	}
	return AudioEffect::setActive (state);
}

tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& newSetup)
{
	auto result = AudioEffect::setupProcessing (newSetup);
	if (result != kResultOk)
		return result;

	sampleRate = std::max (newSetup.sampleRate, 1.0);
	invSampleRate = 1.0 / sampleRate;

	filterL.setup (sampleRate);
	filterR.setup (sampleRate);
	reverb.setup (sampleRate);

	loadSamples ();
	updateEnvelopeIncrements ();
	updateFilterAndReverb ();

	return kResultOk;
}

tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
		return kResultOk;
	return kResultFalse;
}

tresult PLUGIN_API Processor::process (ProcessData& data)
{
	if (!samplesLoaded)
	{
		loadSamples ();
	}

	handleParameterChanges (data.inputParameterChanges);
	handleEvents (data.inputEvents);

	if (data.numOutputs == 0)
		return kResultOk;

	if (data.symbolicSampleSize == kSample32)
		return processAudioFloat (data);
	if (data.symbolicSampleSize == kSample64)
		return processAudioDouble (data);

	return kResultOk;
}

void Processor::handleParameterChanges (IParameterChanges* paramChanges)
{
	if (!paramChanges)
		return;

	bool envelopeDirty = false;
	bool filterDirty = false;
	bool reverbDirty = false;

	for (int32 index = 0, count = paramChanges->getParameterCount (); index < count; ++index)
	{
		auto* queue = paramChanges->getParameterData (index);
		if (!queue)
			continue;

		ParamValue value;
		int32 sampleOffset;
		const int32 pointCount = queue->getPointCount ();
		if (pointCount <= 0)
			continue;
		if (queue->getPoint (pointCount - 1, sampleOffset, value) != kResultTrue)
			continue;

		switch (queue->getParameterId ())
		{
			case kParamAttack:
				parameters.attack = value;
				envelopeDirty = true;
				break;
			case kParamDecay:
				parameters.decay = value;
				envelopeDirty = true;
				break;
			case kParamRelease:
				parameters.release = value;
				envelopeDirty = true;
				break;
			case kParamCutoff:
				parameters.cutoff = value;
				filterDirty = true;
				break;
			case kParamResonance:
				parameters.resonance = value;
				filterDirty = true;
				break;
			case kParamReverbWet:
				parameters.reverbWet = value;
				reverbDirty = true;
				break;
			case kParamReverbSize:
				parameters.reverbSize = value;
				reverbDirty = true;
				break;
			case kParamGain:
				parameters.gain = value;
				break;
			case kParamPlaybackMode:
				parameters.playbackMode = value;
				break;
			default:
				break;
		}
	}

	if (envelopeDirty)
		updateEnvelopeIncrements ();
	if (filterDirty || reverbDirty)
		updateFilterAndReverb ();
}

void Processor::handleEvents (IEventList* events)
{
	if (!events)
		return;

	Event event {};
	for (int32 i = 0, count = events->getEventCount (); i < count; ++i)
	{
		if (events->getEvent (i, event) != kResultTrue)
			continue;
		switch (event.type)
		{
			case Event::kNoteOnEvent:
				onNoteOn (event);
				break;
			case Event::kNoteOffEvent:
				onNoteOff (event);
				break;
			default:
				break;
		}
	}
}

void Processor::onNoteOn (const Event& event)
{
	const auto& noteOn = event.noteOn;
	const int32 midiNote = noteOn.pitch;
	if (samplePool.empty ())
		return;

	const int32 sampleCount = static_cast<int32> (samplePool.size ());
	const int32 mappedIndex = noteToSample[static_cast<size_t> (midiNote) % noteToSample.size ()];
	const int32 sampleIndex = sampleCount > 0 ? mappedIndex % sampleCount : 0;

	Voice* voice = nullptr;
	for (auto& v : voices)
	{
		if (!v.active)
		{
			voice = &v;
			break;
		}
	}
	if (!voice)
	{
		voice = &voices[0];
	}

	voice->active = true;
	voice->noteId = noteOn.noteId;
	voice->midiNote = midiNote;
	voice->sampleIndex = sampleIndex;
	voice->samplePosition = 0.0;
	const auto& sample = samplePool[static_cast<size_t> (sampleIndex)];
	const double srRatio = sample.sourceSampleRate > 0.0 ? sample.sourceSampleRate / sampleRate : 1.0;
	voice->sampleIncrement = std::isfinite (srRatio) && srRatio > 0.0 ? srRatio : 1.0;
	voice->velocity = std::clamp (noteOn.velocity, 0.0f, 1.0f);
	voice->envelope = 0.0;
	voice->stage = Voice::Stage::Attack;
	voice->releaseStartLevel = 1.0;
}

void Processor::onNoteOff (const Event& event)
{
	const auto& noteOff = event.noteOff;

	const bool gateMode = parameters.playbackMode < 0.5;

	for (auto& voice : voices)
	{
		if (!voice.active)
			continue;
		if (voice.noteId == noteOff.noteId || voice.midiNote == noteOff.pitch)
		{
			if (gateMode)
			{
				voice.stage = Voice::Stage::Release;
				voice.releaseStartLevel = std::max (voice.envelope, 1e-5);
			}
			return;
		}
	}
}

void Processor::resetVoices ()
{
	for (auto& voice : voices)
	{
		voice = Voice {};
	}
}

void Processor::loadSamples ()
{
	if (samplesLoaded && !fallbackUsed)
	{
		return;
	}

	fallbackUsed = false;
	if (!loadSamplesFromFilesystem ())
	{
		fallbackUsed = true;
		RYGO_LOG ("RygoSampler: using procedural fallback samples.\n");
	}
	samplesLoaded = true;
}

bool Processor::loadSamplesFromFilesystem ()
{
	std::vector<SampleDefinition> loadedSamples;
	loadedSamples.reserve (kNumSamples);
	bool allLoaded = true;
	const auto searchPaths = buildSampleSearchPaths ();

	for (int32 idx = 1; idx <= kNumSamples; ++idx)
	{
		SampleDefinition sample;
		if (!loadSampleFile (idx, sample, searchPaths))
		{
			allLoaded = false;
			createFallbackSample (idx, sample);
		}
		loadedSamples.push_back (std::move (sample));
	}

	samplePool = std::move (loadedSamples);
	return allLoaded;
}

bool Processor::loadSampleFile (int32 sampleNumber, SampleDefinition& outSample,
                               const std::vector<std::string>& searchPaths)
{
	const auto fileName = makeSampleFileName (sampleNumber);
	for (const auto& basePath : searchPaths)
	{
		const std::string candidate = joinPath (basePath, fileName);
		std::ifstream in (candidate.c_str (), std::ios::binary);
		if (!in)
			continue;

		auto readUInt32 = [&] () -> uint32_t {
			uint8_t buffer[4] {0, 0, 0, 0};
			in.read (reinterpret_cast<char*> (buffer), 4);
			if (!in)
				return 0;
			return static_cast<uint32_t> (buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) |
			                            (buffer[3] << 24));
		};

		char riffId[4] {0};
		in.read (riffId, 4);
		if (!in || std::strncmp (riffId, "RIFF", 4) != 0)
		{
			RYGO_LOG ("RygoSampler: invalid RIFF header in %s\n", candidate.c_str ());
			continue;
		}

		readUInt32 (); // riff size (unused)
		char waveId[4] {0};
		in.read (waveId, 4);
		if (!in || std::strncmp (waveId, "WAVE", 4) != 0)
		{
			RYGO_LOG ("RygoSampler: invalid WAVE header in %s\n", candidate.c_str ());
			continue;
		}

		bool fmtFound = false;
		bool dataFound = false;
		uint16_t audioFormat = 1;
		uint16_t channels = 1;
		uint32_t inputSampleRate = 44100;
		uint16_t bitsPerSample = 16;
		std::vector<uint8_t> dataBuffer;

		while (in && !(fmtFound && dataFound))
		{
			char chunkId[4] {0};
			in.read (chunkId, 4);
			if (!in)
				break;
			uint32_t chunkSize = readUInt32 ();
			if (!in)
				break;

			const std::string chunkName (chunkId, 4);
			if (chunkName == "fmt ")
			{
				if (chunkSize < 16)
					break;
				std::vector<uint8_t> fmtData (chunkSize);
				if (!in.read (reinterpret_cast<char*> (fmtData.data ()), chunkSize))
					break;

				audioFormat = static_cast<uint16_t> (fmtData[0] | (fmtData[1] << 8));
				channels = static_cast<uint16_t> (fmtData[2] | (fmtData[3] << 8));
				inputSampleRate = static_cast<uint32_t> (fmtData[4] | (fmtData[5] << 8) |
				                                       (fmtData[6] << 16) | (fmtData[7] << 24));
				bitsPerSample = static_cast<uint16_t> (fmtData[14] | (fmtData[15] << 8));
				fmtFound = true;
			}
			else if (chunkName == "data")
			{
				dataBuffer.resize (chunkSize);
				if (!in.read (reinterpret_cast<char*> (dataBuffer.data ()), chunkSize))
					break;
				dataFound = true;
			}
			else
			{
				in.seekg (static_cast<std::streamoff> (chunkSize), std::ios::cur);
			}

			if (chunkSize % 2 == 1)
			{
				in.seekg (1, std::ios::cur);
			}
		}

		if (!fmtFound || !dataFound || channels == 0)
		{
			RYGO_LOG ("RygoSampler: incomplete WAV chunk data in %s\n", candidate.c_str ());
			continue;
		}

		const size_t bytesPerSample = bitsPerSample / 8;
		if (bytesPerSample == 0)
			continue;

		const size_t frameSize = bytesPerSample * static_cast<size_t> (channels);
		if (frameSize == 0 || dataBuffer.size () < frameSize)
			continue;

		const size_t frameCount = dataBuffer.size () / frameSize;
		if (frameCount == 0)
			continue;

		outSample.data.resize (frameCount);
		outSample.sourceSampleRate = inputSampleRate > 0 ? static_cast<double> (inputSampleRate) : 44100.0;

		for (size_t frame = 0; frame < frameCount; ++frame)
		{
			const uint8_t* framePtr = dataBuffer.data () + frame * frameSize;
			double accumulated = 0.0;
			for (uint16_t ch = 0; ch < channels; ++ch)
			{
				const uint8_t* samplePtr = framePtr + ch * bytesPerSample;
				double value = 0.0;
				switch (bitsPerSample)
				{
					case 8:
					{
						const double raw = static_cast<double> (samplePtr[0]) - 128.0;
						value = raw / 128.0;
						break;
					}
					case 16:
					{
						int16_t raw = static_cast<int16_t> (samplePtr[0] | (samplePtr[1] << 8));
						value = static_cast<double> (raw) / 32768.0;
						break;
					}
					case 24:
					{
						int32_t raw = samplePtr[0] | (samplePtr[1] << 8) | (samplePtr[2] << 16);
						if (raw & 0x800000)
							raw |= ~0xFFFFFF;
						value = static_cast<double> (raw) / 8388608.0;
						break;
					}
					case 32:
					{
						if (audioFormat == 3)
						{
							float raw = 0.f;
							std::memcpy (&raw, samplePtr, sizeof (float));
							value = static_cast<double> (raw);
						}
						else
						{
							int32_t raw = samplePtr[0] | (samplePtr[1] << 8) | (samplePtr[2] << 16) |
							             (samplePtr[3] << 24);
							value = static_cast<double> (raw) / 2147483648.0;
						}
						break;
					}
					default:
						value = 0.0;
						break;
				}
				accumulated += value;
			}

			double mono = accumulated / static_cast<double> (channels);
			mono = std::clamp (mono, -1.0, 1.0);
			outSample.data[frame] = static_cast<float> (mono);
		}

		if (!outSample.data.empty ())
		{
			const size_t fadeSamples = std::min<size_t> (outSample.data.size () / 50, 512);
			for (size_t i = 0; i < fadeSamples; ++i)
			{
				const double gain = static_cast<double> (i) / fadeSamples;
				outSample.data[i] *= static_cast<float> (gain);
				outSample.data[outSample.data.size () - 1 - i] *= static_cast<float> (gain);
			}
		}

		return true;
	}

	RYGO_LOG ("RygoSampler: missing sample %d in search paths\n", sampleNumber);
	return false;
}

void Processor::createFallbackSample (int32 sampleNumber, SampleDefinition& outSample)
{
	const double fallbackSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
	outSample.sourceSampleRate = fallbackSampleRate;
	const size_t length = static_cast<size_t> (kFallbackDurationSeconds * fallbackSampleRate);
	const size_t safeLength = std::max<size_t> (length, static_cast<size_t> (fallbackSampleRate * 0.2));
	outSample.data.resize (safeLength);

	const double baseFrequency = 110.0 + (sampleNumber % 12) * 35.0;
	for (size_t n = 0; n < safeLength; ++n)
	{
		double t = static_cast<double> (n) / fallbackSampleRate;
		double env = std::exp (-3.5 * t / kFallbackDurationSeconds);
		double tone = std::sin (kTwoPi * baseFrequency * t);
		double sampleValue = tone * env;
		outSample.data[n] = static_cast<float> (sampleValue);
	}

	const size_t fadeSamples = std::max<size_t> (1, static_cast<size_t> (safeLength * kFallbackFadeFraction));
	for (size_t i = 0; i < fadeSamples && i < outSample.data.size (); ++i)
	{
		double gain = static_cast<double> (i) / fadeSamples;
		outSample.data[i] *= static_cast<float> (gain);
		outSample.data[outSample.data.size () - 1 - i] *= static_cast<float> (gain);
	}
}

void Processor::updateNoteMapping ()
{
	std::array<int32, kNumSamples> indices {};
	std::iota (indices.begin (), indices.end (), 0);

	std::mt19937 rng (424242);
	std::shuffle (indices.begin (), indices.end (), rng);

	for (size_t note = 0; note < noteToSample.size (); ++note)
	{
		noteToSample[note] = indices[note % kNumSamples];
	}
}

void Processor::updateFilterAndReverb ()
{
	filterL.setLowPass (cutoffHz (), resonanceQ ());
	filterR.setLowPass (cutoffHz (), resonanceQ ());
	reverb.setParameters (parameters.reverbSize, parameters.reverbWet);
}

void Processor::updateEnvelopeIncrements ()
{
	const auto atk = attackTimeSeconds ();
	const auto dec = std::max (decayTimeSeconds (), 1e-5);
	const auto rel = std::max (releaseTimeSeconds (), 1e-5);

	attackIncrement = atk <= 1e-5 ? 1.0 : invSampleRate / atk;
	decayIncrement = invSampleRate / dec;
	releaseIncrement = invSampleRate / rel;
}

double Processor::attackTimeSeconds () const
{
	return 0.001 + std::pow (parameters.attack, 2.0) * 3.0;
}

double Processor::decayTimeSeconds () const
{
	return 0.001 + std::pow (parameters.decay, 2.0) * 3.0;
}

double Processor::releaseTimeSeconds () const
{
	return 0.005 + std::pow (parameters.release, 2.0) * 5.0;
}

double Processor::cutoffHz () const
{
	const double minHz = 80.0;
	const double maxHz = 18000.0;
	const double factor = std::pow (maxHz / minHz, std::clamp (parameters.cutoff, 0.0, 1.0));
	return minHz * factor;
}

double Processor::resonanceQ () const
{
	return 0.5 + parameters.resonance * 9.5;
}

template <typename SampleType>
tresult Processor::processAudioImpl (ProcessData& data)
{
	if (data.numSamples == 0)
		return kResultOk;

	auto& output = data.outputs[0];
	const int32 numChannels = std::min<int32> (output.numChannels, 2);
	if (numChannels == 0)
		return kResultOk;

	SampleType** channels = nullptr;
	if constexpr (std::is_same_v<SampleType, float>)
		channels = output.channelBuffers32;
	else
		channels = output.channelBuffers64;

	for (int32 ch = 0; ch < numChannels; ++ch)
	{
		std::fill (channels[ch], channels[ch] + data.numSamples, static_cast<SampleType> (0));
	}

	// Gain: normalized 0..1 maps to -6..+6 dB (0.5 = 0 dB)
	const double gainDB = parameters.gain * 12.0 - 6.0;
	const double gainLinear = std::pow (10.0, gainDB / 20.0);

	for (int32 frame = 0; frame < data.numSamples; ++frame)
	{
		double mixL = 0.0;
		double mixR = 0.0;

		for (auto& voice : voices)
		{
			if (!voice.active)
				continue;

			if (voice.sampleIndex < 0 ||
			    static_cast<size_t> (voice.sampleIndex) >= samplePool.size ())
			{
				voice.active = false;
				continue;
			}

			const auto& sample = samplePool[static_cast<size_t> (voice.sampleIndex)];
			const auto& pcm = sample.data;
			if (pcm.empty ())
			{
				voice.active = false;
				continue;
			}

			const size_t sampleSize = pcm.size ();
			if (sampleSize == 0)
			{
				voice.active = false;
				continue;
			}

			if (voice.stage == Voice::Stage::Release && voice.envelope <= 0.0)
			{
				voice.active = false;
				continue;
			}

			double clampedPosition = std::clamp (voice.samplePosition, 0.0,
			                                 static_cast<double> (sampleSize - 1));
			size_t index = static_cast<size_t> (clampedPosition);
			if (index >= sampleSize)
			{
				index = sampleSize - 1;
				voice.samplePosition = static_cast<double> (index);
				if (voice.stage != Voice::Stage::Release)
				{
					voice.stage = Voice::Stage::Release;
					voice.releaseStartLevel = std::max (voice.envelope, 1e-5);
				}
			}

			switch (voice.stage)
			{
				case Voice::Stage::Attack:
					voice.envelope += attackIncrement;
					if (voice.envelope >= 1.0 || attackIncrement >= 1.0)
					{
						voice.envelope = 1.0;
						voice.stage = Voice::Stage::Decay;
					}
					break;
				case Voice::Stage::Decay:
				{
					// Decay falls from 1.0 to 0.0 (Sustain param removed)
					voice.envelope -= decayIncrement;
					if (voice.envelope <= 0.0)
					{
						voice.envelope = 0.0;
						voice.stage = Voice::Stage::Release;
						voice.releaseStartLevel = 1e-5;
					}
					break;
				}
				case Voice::Stage::Sustain:
					// Sustain removed; deactivate voice
					voice.active = false;
					continue;
				case Voice::Stage::Release:
				{
					const double delta =
					    releaseIncrement * std::max (voice.releaseStartLevel, 1e-4);
					voice.envelope = std::max (0.0, voice.envelope - delta);
					if (voice.envelope <= 1e-4)
					{
						voice.envelope = 0.0;
						voice.active = false;
						continue;
					}
					break;
				}
				case Voice::Stage::Idle:
				default:
					voice.active = false;
					continue;
			}

			const size_t nextIndex = std::min (index + 1, sampleSize - 1);
			const double frac = clampedPosition - static_cast<double> (index);
			const double current =
			    static_cast<double> (pcm[index]) +
			    (static_cast<double> (pcm[nextIndex]) - static_cast<double> (pcm[index])) * frac;
			const double amplitude = std::clamp (voice.envelope, 0.0, 1.0) * voice.velocity;
			const double voiceSample = current * amplitude;

			mixL += voiceSample;
			mixR += voiceSample;

			voice.samplePosition += voice.sampleIncrement;
			if (voice.samplePosition >= static_cast<double> (sampleSize))
			{
				voice.samplePosition = static_cast<double> (sampleSize);
				if (voice.stage != Voice::Stage::Release)
				{
					voice.stage = Voice::Stage::Release;
					voice.releaseStartLevel = std::max (voice.envelope, 1e-5);
				}
			}
		}

		const double filteredL = filterL.process (mixL);
		const double filteredR = filterR.process (mixR);
		double outL = 0.0;
		double outR = 0.0;
		reverb.process (filteredL, filteredR, outL, outR);

		channels[0][frame] = static_cast<SampleType> (outL * gainLinear);
		if (numChannels > 1)
			channels[1][frame] = static_cast<SampleType> (outR * gainLinear);
	}

	output.silenceFlags = 0;

	return kResultOk;
}

tresult Processor::processAudioFloat (ProcessData& data)
{
	return processAudioImpl<float> (data);
}

tresult Processor::processAudioDouble (ProcessData& data)
{
	return processAudioImpl<double> (data);
}

template tresult Processor::processAudioImpl<float> (ProcessData& data);
template tresult Processor::processAudioImpl<double> (ProcessData& data);

tresult PLUGIN_API Processor::setState (IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	int32 version = 0;
	if (!s.readInt32 (version) || version != 2)
	{
		// Unknown or old format — keep defaults
		return kResultOk;
	}
	if (!s.readDouble (parameters.attack)) return kResultFalse;
	if (!s.readDouble (parameters.decay)) return kResultFalse;
	if (!s.readDouble (parameters.release)) return kResultFalse;
	if (!s.readDouble (parameters.cutoff)) return kResultFalse;
	if (!s.readDouble (parameters.resonance)) return kResultFalse;
	if (!s.readDouble (parameters.reverbWet)) return kResultFalse;
	if (!s.readDouble (parameters.reverbSize)) return kResultFalse;
	if (!s.readDouble (parameters.gain)) return kResultFalse;
	if (!s.readDouble (parameters.playbackMode)) return kResultFalse;

	updateEnvelopeIncrements ();
	updateFilterAndReverb ();
	return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* stream)
{
	IBStreamer s (stream, kLittleEndian);
	if (!s.writeInt32 (2)) return kResultFalse;  // state format version 2
	if (!s.writeDouble (parameters.attack)) return kResultFalse;
	if (!s.writeDouble (parameters.decay)) return kResultFalse;
	if (!s.writeDouble (parameters.release)) return kResultFalse;
	if (!s.writeDouble (parameters.cutoff)) return kResultFalse;
	if (!s.writeDouble (parameters.resonance)) return kResultFalse;
	if (!s.writeDouble (parameters.reverbWet)) return kResultFalse;
	if (!s.writeDouble (parameters.reverbSize)) return kResultFalse;
	if (!s.writeDouble (parameters.gain)) return kResultFalse;
	if (!s.writeDouble (parameters.playbackMode)) return kResultFalse;
	return kResultOk;
}

} // namespace Steinberg::Vst::RygoSampler
