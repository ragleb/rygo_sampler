#include "SampleLibrary.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cstdlib>

namespace rygo
{
namespace
{
constexpr const char* scaleKey = "uiScale";

double noteFromHz (double hz)
{
    return 69.0 + 12.0 * std::log2 (hz / 440.0);
}

void analyse (SampleData& sample)
{
    const auto& audio = sample.audio;
    const auto numFrames = audio.getNumSamples();
    const auto rate = sample.sourceRate;

    sample.fileSeconds = numFrames / rate;
    sample.peak = audio.getMagnitude (0, numFrames);

    // Silence is judged relative to the sample's own peak: these files peak anywhere from
    // -30 to -4 dBFS, and a fixed threshold would make quiet samples look shorter than they sound.
    const auto threshold = juce::jmax (sample.peak * 0.00316f, 0.0001f);
    const auto window = juce::jmax (1, juce::roundToInt (rate * 0.01));

    int first = -1;
    int last = -1;
    for (int frame = 0; frame < numFrames; frame += window)
    {
        if (audio.getMagnitude (frame, juce::jmin (window, numFrames - frame)) > threshold)
        {
            if (first < 0)
                first = frame;
            last = juce::jmin (frame + window, numFrames);
        }
    }

    if (first < 0)
    {
        first = 0;
        last = numFrames;
    }

    sample.audibleSeconds = (last - first) / rate;
    sample.trimStart = juce::jmax (0, first - juce::roundToInt (rate * 0.002));
    sample.trimEnd = juce::jmin (numFrames, last + juce::roundToInt (rate * 0.03));
    sample.normGain = sample.peak > 0.00001f ? juce::jmin (0.5f / sample.peak, 31.6f) : 1.0f;

    const auto pitch = estimatePitch (audio, rate, first, last);
    if (pitch.hz > 0.0)
    {
        sample.rootNote = juce::jlimit (0, 127, juce::roundToInt (noteFromHz (pitch.hz)));
        sample.hasClearPitch = pitch.isClear;
    }

    sample.overview.assign ((size_t) SampleData::overviewBins, 0.0f);
    for (int bin = 0; bin < SampleData::overviewBins; ++bin)
    {
        const auto from = (int) ((juce::int64) numFrames * bin / SampleData::overviewBins);
        const auto to = (int) ((juce::int64) numFrames * (bin + 1) / SampleData::overviewBins);
        if (to > from)
            sample.overview[(size_t) bin] = audio.getMagnitude (from, to - from);
    }
}
}

namespace
{
/** Normalised square difference (McLeod): tells a real period from a mere loud patch of noise.
    Returns the fundamental in Hz, or 0 if there isn't one.
*/
double fundamentalHz (const std::vector<float>& mono, double lowRate)
{
    const auto count = (int) mono.size();
    const auto minLag = juce::jmax (2, juce::roundToInt (lowRate / 1200.0));
    const auto maxLag = juce::roundToInt (lowRate / 50.0);
    const auto window = juce::jmin (juce::roundToInt (lowRate * 0.08), count - maxLag);

    if (window < minLag * 4)
        return 0.0;

    const auto energyAt = [&] (int position)
    {
        auto energy = 0.0f;
        for (int i = 0; i < window; ++i)
            energy += mono[(size_t) (position + i)] * mono[(size_t) (position + i)];
        return energy;
    };

    constexpr int maxFrames = 5;
    const auto span = count - window - maxLag;
    auto loudest = 0.0f;
    std::array<float, maxFrames> energies {};
    for (int frame = 0; frame < maxFrames; ++frame)
    {
        energies[(size_t) frame] = energyAt (span * frame / (maxFrames - 1));
        loudest = juce::jmax (loudest, energies[(size_t) frame]);
    }

    if (loudest <= 0.0f)
        return 0.0;

    std::vector<float> nsdf ((size_t) maxLag + 2, 0.0f);
    std::vector<double> estimates;
    int framesTried = 0;

    for (int frame = 0; frame < maxFrames; ++frame)
    {
        // Quiet tails say little about the pitch of what the ear perceives.
        if (energies[(size_t) frame] < loudest * 0.1f)
            continue;

        ++framesTried;
        const auto* x = mono.data() + span * frame / (maxFrames - 1);

        for (int lag = minLag; lag <= maxLag + 1; ++lag)
        {
            auto product = 0.0f, power = 0.0f;
            for (int i = 0; i < window; ++i)
            {
                product += x[i] * x[i + lag];
                power += x[i] * x[i] + x[i + lag] * x[i + lag];
            }
            nsdf[(size_t) lag] = power > 0.0f ? 2.0f * product / power : 0.0f;
        }

        // The tallest peak of each positive lobe; the first close to the highest is the fundamental,
        // not one of its multiples.
        auto highest = 0.0f;
        for (int lag = minLag + 1; lag <= maxLag; ++lag)
            if (nsdf[(size_t) lag] > nsdf[(size_t) lag - 1] && nsdf[(size_t) lag] >= nsdf[(size_t) lag + 1])
                highest = juce::jmax (highest, nsdf[(size_t) lag]);

        if (highest < 0.8f)
            continue;

        for (int lag = minLag + 1; lag <= maxLag; ++lag)
        {
            const auto value = nsdf[(size_t) lag];
            if (value > nsdf[(size_t) lag - 1] && value >= nsdf[(size_t) lag + 1] && value >= highest * 0.9f)
            {
                const auto before = nsdf[(size_t) lag - 1], after = nsdf[(size_t) lag + 1];
                const auto curve = before - 2.0f * value + after;
                const auto refined = (double) lag + (curve < 0.0f ? 0.5 * (before - after) / curve : 0.0);
                estimates.push_back (noteFromHz (lowRate / refined));
                break;
            }
        }
    }

    // A real pitch shows up in most of the sound and in the same place.
    if (framesTried == 0 || (int) estimates.size() * 2 <= framesTried)
        return 0.0;

    std::sort (estimates.begin(), estimates.end());
    const auto median = estimates[estimates.size() / 2];
    const auto agreeing = std::count_if (estimates.begin(), estimates.end(),
                                         [median] (double note) { return std::abs (note - median) < 1.0; });

    if ((int) agreeing * 2 <= framesTried)
        return 0.0;

    return 440.0 * std::pow (2.0, (median - 69.0) / 12.0);
}

/** The strongest frequency across the whole sound: the note a noisy sound seems to sit on. */
double strongestFrequencyHz (const std::vector<float>& mono, double lowRate)
{
    constexpr int order = 11;
    constexpr int size = 1 << order;

    const auto count = (int) mono.size();
    if (count < 64)
        return 0.0;

    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann);

    std::vector<float> frame ((size_t) size * 2);
    std::vector<float> spectrum ((size_t) size / 2 + 1, 0.0f);

    constexpr int maxFrames = 8;
    const auto span = juce::jmax (0, count - size);
    const auto frames = span > 0 ? maxFrames : 1;

    for (int f = 0; f < frames; ++f)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        const auto start = frames > 1 ? span * f / (frames - 1) : 0;
        const auto length = juce::jmin (size, count - start);
        std::copy_n (mono.data() + start, length, frame.data());
        window.multiplyWithWindowingTable (frame.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (frame.data());

        // Summed as they are: louder moments count for more, as they do to the ear.
        for (size_t bin = 0; bin < spectrum.size(); ++bin)
            spectrum[bin] += frame[bin];
    }

    const auto binHz = lowRate / size;
    const auto firstBin = juce::jmax (1, juce::roundToInt (50.0 / binHz));
    const auto lastBin = juce::jmin ((int) spectrum.size() - 2, juce::roundToInt (4000.0 / binHz));

    int peak = -1;
    auto strongest = 0.0f;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        if (spectrum[(size_t) bin] > strongest)
        {
            strongest = spectrum[(size_t) bin];
            peak = bin;
        }
    }

    if (peak < 0)
        return 0.0;

    const auto before = spectrum[(size_t) peak - 1], value = spectrum[(size_t) peak], after = spectrum[(size_t) peak + 1];
    const auto curve = before - 2.0f * value + after;
    const auto refined = (double) peak + (curve < 0.0f ? 0.5 * (before - after) / curve : 0.0);
    return refined * binHz;
}
}

PitchEstimate estimatePitch (const juce::AudioBuffer<float>& audio, double sampleRate, int firstFrame, int lastFrame)
{
    // 12 kHz is plenty for finding a pitch, and much faster to search.
    const auto factor = juce::jmax (1, juce::roundToInt (sampleRate / 12000.0));
    const auto lowRate = sampleRate / factor;
    const auto count = (lastFrame - firstFrame) / factor;

    if (count < 16)
        return {};

    std::vector<float> mono ((size_t) count, 0.0f);
    const auto channels = audio.getNumChannels();
    for (int i = 0; i < count; ++i)
    {
        auto sum = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            for (int f = 0; f < factor; ++f)
                sum += audio.getSample (channel, firstFrame + i * factor + f);
        mono[(size_t) i] = sum / (float) (factor * channels);
    }

    if (const auto hz = fundamentalHz (mono, lowRate); hz > 0.0)
        return { hz, true };

    return { strongestFrequencyHz (mono, lowRate), false };
}

SampleLibrary::SampleLibrary()
{
    formats.registerBasicFormats();
}

int SampleLibrary::firstNoteForCount (int count)
{
    // Centre the samples on the keyboard, starting from a C so the layout is easy to remember.
    const auto centred = juce::jmax (0, (maxKeys - count) / 2);
    return centred - centred % 12;
}

juce::File SampleLibrary::getFactoryRoot() const
{
    if (const auto* overridePath = std::getenv ("RYGO_FACTORY_SAMPLES"))
        return juce::File (juce::String::fromUTF8 (overridePath));

    // Plugin binary lives in <bundle>/Contents/<platform>/; samples are in Contents/Resources/Samples.
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int depth = 0; depth < 3; ++depth)
    {
        const auto candidate = dir.getChildFile ("Resources").getChildFile ("Samples");
        if (candidate.isDirectory())
            return candidate;
        dir = dir.getParentDirectory();
    }
    return {};
}

juce::PropertiesFile& SampleLibrary::settings()
{
    if (settingsFile == nullptr)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "settings";
        options.filenameSuffix = ".xml";
        options.folderName = "ZHRI Records/Rygostancija";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;

        if (const auto* overrideDir = std::getenv ("RYGO_SETTINGS_DIR"))
            settingsFile = std::make_unique<juce::PropertiesFile> (
                juce::File (juce::String::fromUTF8 (overrideDir)).getChildFile ("settings.xml"), options);
        else
            settingsFile = std::make_unique<juce::PropertiesFile> (options);
    }
    return *settingsFile;
}

double SampleLibrary::getUiScale()
{
    const juce::ScopedLock scoped (lock);
    return settings().getDoubleValue (scaleKey, 1.0);
}

void SampleLibrary::setUiScale (double scale)
{
    const juce::ScopedLock scoped (lock);
    settings().setValue (scaleKey, scale);
}

std::vector<juce::File> SampleLibrary::findAudioFiles (const juce::File& folder) const
{
    std::vector<juce::File> files;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles | juce::File::ignoreHiddenFiles))
    {
        const auto file = entry.getFile();
        if (file.hasFileExtension ("wav;wave;aif;aiff;flac;ogg;caf;m4a;mp3") && ! file.getFullPathName().contains ("__MACOSX"))
            files.push_back (file);
    }
    std::sort (files.begin(), files.end());
    return files;
}

SamplePtr SampleLibrary::loadSample (const juce::File& file, juce::StringArray& notes)
{
    const auto key = file.getFullPathName() + "|" + juce::String (file.getLastModificationTime().toMilliseconds())
                     + "|" + juce::String (file.getSize());
    {
        const juce::ScopedLock scoped (lock);
        const auto found = cache.find (key);
        if (found != cache.end())
            if (auto cached = found->second.lock())
                return cached;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    {
        notes.add ("Could not read " + file.getFileName());
        return nullptr;
    }

    if ((double) reader->lengthInSamples / reader->sampleRate > maxSecondsPerFile)
    {
        notes.add ("Skipped a file that is too long: " + file.getFileName());
        return nullptr;
    }

    auto sample = std::make_shared<SampleData>();
    sample->name = file.getFileNameWithoutExtension();
    sample->sourceRate = reader->sampleRate;
    sample->audio.setSize (juce::jlimit (1, 2, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&sample->audio, 0, (int) reader->lengthInSamples, 0, true, true);
    analyse (*sample);

    SamplePtr result = sample;
    {
        const juce::ScopedLock scoped (lock);
        for (auto it = cache.begin(); it != cache.end();)
            it = it->second.expired() ? cache.erase (it) : std::next (it);
        cache[key] = result;
    }
    return result;
}

BankPtr SampleLibrary::loadBank (const std::function<bool()>& shouldCancel)
{
    auto bank = std::make_shared<Bank>();
    bank->noteToSample.fill (-1);

    const auto folder = getFactoryRoot();
    if (! folder.isDirectory())
    {
        bank->folderMissing = true;
        bank->notes.add ("Samples not found. Please reinstall the plugin.");
        return bank;
    }

    for (const auto& file : findAudioFiles (folder))
    {
        if (shouldCancel && shouldCancel())
            return nullptr;
        if (auto sample = loadSample (file, bank->notes))
            bank->samples.push_back (std::move (sample));
    }

    std::stable_sort (bank->samples.begin(), bank->samples.end(), [] (const SamplePtr& a, const SamplePtr& b)
    {
        if (! juce::exactlyEqual (a->audibleSeconds, b->audibleSeconds))
            return a->audibleSeconds > b->audibleSeconds;
        return a->name.compareNatural (b->name) < 0;
    });

    if ((int) bank->samples.size() > maxKeys)
    {
        bank->notes.add ("Only the 128 longest samples fit on the keyboard");
        bank->samples.resize ((size_t) maxKeys);
    }

    bank->firstNote = firstNoteForCount ((int) bank->samples.size());
    for (size_t i = 0; i < bank->samples.size(); ++i)
        bank->noteToSample[(size_t) bank->firstNote + i] = (int) i;

    return bank;
}
}
