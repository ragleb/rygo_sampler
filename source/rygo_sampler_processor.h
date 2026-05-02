#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <array>
#include <vector>

namespace Steinberg::Vst::RygoSampler {

constexpr int32 kNumSamples = 120;
constexpr int32 kMaxVoices = 32;

class BiquadFilter
{
public:
	void setup (double sampleRate);
	void setLowPass (double cutoffHz, double q);
	void reset ();

	double process (double input);

private:
	double sampleRate {44100.0};
	double b0 {1.0};
	double b1 {0.0};
	double b2 {0.0};
	double a1 {0.0};
	double a2 {0.0};
	double z1 {0.0};
	double z2 {0.0};
};

class SimpleReverb
{
public:
	void setup (double sampleRate);
	void setParameters (double sizeNormalized, double wetNormalized);
	void reset ();

	void process (double inL, double inR, double& outL, double& outR);

private:
	static constexpr int32 kDelayCount = 4;

	struct DelayLine
	{
		std::vector<double> buffer;
		size_t index {0};
		double feedback {0.6};

		void resize (size_t newSize);
		double process (double input);
		void reset ();
	};

	double sampleRate {44100.0};
	double wet {0.25};
	double size {0.5};
	std::array<DelayLine, kDelayCount> delaysL;
	std::array<DelayLine, kDelayCount> delaysR;

	void updateDelayLengths ();
};

class Processor : public AudioEffect
{
public:
	Processor ();

	static FUnknown* createInstance (void*);

	//--- AudioEffect overrides
	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;
	tresult PLUGIN_API setupProcessing (ProcessSetup& setup) SMTG_OVERRIDE;
	tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;
	tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;

protected:
	struct ParameterState
	{
		double attack {0.0};       // normalized 0..1
		double decay {1.0};
		double release {0.0};
		double cutoff {1.0};
		double resonance {0.0};
		double reverbWet {0.0};
		double reverbSize {0.0};
		double playbackMode {0.0};
		double gain {0.5};         // normalized 0..1 → -6..+6 dB (0.5 = 0 dB)
	};

	struct SampleDefinition
	{
		std::vector<float> data;
		double sourceSampleRate {44100.0};
	};

	struct Voice
	{
		bool active {false};
		int32 noteId {-1};
		int32 midiNote {0};
		int32 sampleIndex {0};
		double samplePosition {0.0};
		double sampleIncrement {1.0};
		double velocity {1.0};
		double envelope {0.0};
		enum class Stage
		{
			Idle,
			Attack,
			Decay,
			Sustain,
			Release
		} stage {Stage::Idle};
		double releaseStartLevel {1.0};
	};

	void resetVoices ();
	void loadSamples ();
	bool loadSamplesFromFilesystem ();
	bool loadSampleFile (int32 sampleNumber, SampleDefinition& outSample,
	                     const std::vector<std::string>& searchPaths);
	void createFallbackSample (int32 sampleNumber, SampleDefinition& outSample);
	void updateNoteMapping ();
	void handleParameterChanges (IParameterChanges* paramChanges);
	void handleEvents (IEventList* events);
	void onNoteOn (const Event& event);
	void onNoteOff (const Event& event);

	template <typename SampleType>
	tresult processAudioImpl (ProcessData& data);
	tresult processAudioFloat (ProcessData& data);
	tresult processAudioDouble (ProcessData& data);

	void updateFilterAndReverb ();
	void updateEnvelopeIncrements ();

	double attackTimeSeconds () const;
	double decayTimeSeconds () const;
	double releaseTimeSeconds () const;
	double cutoffHz () const;
	double resonanceQ () const;

	ParameterState parameters;
	double sampleRate {44100.0};
	double invSampleRate {1.0 / 44100.0};

	std::vector<SampleDefinition> samplePool;
		std::array<int32, 128> noteToSample {};
		std::array<Voice, kMaxVoices> voices;

		BiquadFilter filterL;
		BiquadFilter filterR;
		SimpleReverb reverb;

		double attackIncrement {0.0};
		double decayIncrement {0.0};
		double releaseIncrement {0.0};

		bool samplesLoaded {false};
		bool fallbackUsed {false};
	};

} // namespace Steinberg::Vst::RygoSampler
