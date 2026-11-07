#include "ArtGenerator.h"
#include "PluginEditor.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

// Counting allocations on the calling thread proves the audio path never touches the heap.
namespace
{
thread_local bool countAllocations = false;
thread_local int allocationCount = 0;
}

void* operator new (std::size_t size)
{
    if (countAllocations)
        ++allocationCount;
    if (auto* p = std::malloc (size != 0 ? size : 1))
        return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace rygo;
using juce::AudioBuffer;
using juce::String;

namespace
{
constexpr int oneShot = 0, gate = 1, loop = 2;

int passed = 0, failed = 0;

void check (bool condition, const String& what)
{
    if (condition)
    {
        ++passed;
        std::cout << "  ok    " << what << std::endl;
    }
    else
    {
        ++failed;
        std::cout << "  FAIL  " << what << std::endl;
    }
}

void section (const char* title) { std::cout << "\n" << title << std::endl; }

struct Note { double time; int note; float velocity; double length; };

class Rig
{
public:
    explicit Rig (double rate = 48000.0, int blockSize = 512) : sampleRate (rate), block (blockSize)
    {
        proc.setNonRealtime (true);
        proc.setPlayConfigDetails (0, 2, rate, blockSize);
        proc.prepareToPlay (rate, blockSize);
    }

    void set (const char* id, float value)
    {
        auto* parameter = proc.getState().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    float get (const char* id) { return proc.getState().getRawParameterValue (id)->load(); }

    /** The order a host uses when opening a project: settings first, then playback starts. */
    void startPlayback() { proc.prepareToPlay (sampleRate, block); }

    AudioBuffer<float> render (const std::vector<Note>& notes, double seconds,
                               const std::function<void (int)>& perBlock = {})
    {
        const auto total = juce::roundToInt (seconds * sampleRate);
        AudioBuffer<float> out (2, total);
        out.clear();

        std::vector<std::pair<int, juce::MidiMessage>> events;
        for (const auto& n : notes)
        {
            events.push_back ({ juce::roundToInt (n.time * sampleRate), juce::MidiMessage::noteOn (1, n.note, n.velocity) });
            events.push_back ({ juce::roundToInt ((n.time + n.length) * sampleRate), juce::MidiMessage::noteOff (1, n.note) });
        }
        std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        for (int start = 0; start < total; start += block)
        {
            const auto count = juce::jmin (block, total - start);
            AudioBuffer<float> chunk (2, count);
            juce::MidiBuffer midi;
            for (const auto& [position, message] : events)
                if (position >= start && position < start + count)
                    midi.addEvent (message, position - start);

            if (perBlock)
                perBlock (start);

            proc.processBlock (chunk, midi);
            out.copyFrom (0, start, chunk, 0, 0, count);
            out.copyFrom (1, start, chunk, 1, 0, count);
        }
        return out;
    }

    RygoProcessor proc;
    double sampleRate;
    int block;
};

/** Makes the plugins created in this scope load the samples from another folder. */
class ScopedSamples
{
public:
    explicit ScopedSamples (const juce::File& folder)
        : previous (std::getenv ("RYGO_FACTORY_SAMPLES"))
    {
        setenv ("RYGO_FACTORY_SAMPLES", folder.getFullPathName().toRawUTF8(), 1);
    }

    ~ScopedSamples()
    {
        setenv ("RYGO_FACTORY_SAMPLES", previous.toRawUTF8(), 1);
    }

private:
    String previous;
};

float rmsOf (const AudioBuffer<float>& b, int start, int count)
{
    start = juce::jlimit (0, b.getNumSamples(), start);
    count = juce::jlimit (0, b.getNumSamples() - start, count);
    return count > 0 ? juce::jmax (b.getRMSLevel (0, start, count), b.getRMSLevel (1, start, count)) : 0.0f;
}

bool isFinite (const AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i)))
                return false;
    return true;
}

int lastAbove (const AudioBuffer<float>& b, float threshold)
{
    for (int i = b.getNumSamples() - 1; i >= 0; --i)
        if (std::abs (b.getSample (0, i)) > threshold || std::abs (b.getSample (1, i)) > threshold)
            return i;
    return -1;
}

int firstAbove (const AudioBuffer<float>& b, float threshold)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
        if (std::abs (b.getSample (0, i)) > threshold || std::abs (b.getSample (1, i)) > threshold)
            return i;
    return -1;
}

float maxDifference (const AudioBuffer<float>& a, const AudioBuffer<float>& b)
{
    auto worst = 0.0f;
    const auto n = juce::jmin (a.getNumSamples(), b.getNumSamples());
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
            worst = juce::jmax (worst, std::abs (a.getSample (c, i) - b.getSample (c, i)));
    return worst;
}

double crossingsPerSecond (const AudioBuffer<float>& b, int channel, int start, int count, double rate)
{
    int crossings = 0;
    for (int i = start + 1; i < start + count; ++i)
        if ((b.getSample (channel, i - 1) < 0.0f) != (b.getSample (channel, i) < 0.0f))
            ++crossings;
    return crossings * rate / count;
}

std::vector<Note> phrase()
{
    return { { 0.0, 60, 1.0f, 0.2 }, { 0.25, 62, 1.0f, 0.2 }, { 0.5, 64, 1.0f, 0.2 }, { 0.75, 70, 1.0f, 0.2 } };
}

juce::File tempFolder (const String& name)
{
    auto folder = juce::File::createTempFile ("").getSiblingFile (name);
    folder.deleteRecursively();
    folder.createDirectory();
    return folder;
}

void writeWav (const juce::File& file, double rate, double seconds, double freqLeft, double freqRight, int channels = 2)
{
    file.getParentDirectory().createDirectory();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (rate)
                                                   .withNumChannels (channels)
                                                   .withBitsPerSample (24));
    const auto frames = juce::roundToInt (rate * seconds);
    AudioBuffer<float> audio (channels, frames);
    for (int i = 0; i < frames; ++i)
    {
        audio.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * freqLeft * i / rate));
        if (channels > 1)
            audio.setSample (1, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * freqRight * i / rate));
    }
    writer->writeFromAudioSampleBuffer (audio, 0, frames);
}

bool isAscii (const String& text)
{
    for (auto c : text)
        if ((int) c > 126)
            return false;
    return true;
}

//==============================================================================
void testSamples (const BankPtr& bank)
{
    section ("1. Samples");

    check (bank->samples.size() == 121, "all 121 samples are found, including Rygo_121 that the old plugin ignored");
    check (bank->firstNote == 0, "they start on the lowest key, a C");
    check (bank->notes.isEmpty(), "every file could be read");
}

void testMapping (const BankPtr& bank)
{
    section ("2. Keyboard mapping: higher note, shorter sample");

    bool ordered = true, contiguous = true;
    double previous = 1.0e9;
    int mapped = 0;

    for (int note = 0; note < 128; ++note)
    {
        if (const auto* sample = bank->sampleForNote (note))
        {
            ++mapped;
            ordered = ordered && sample->audibleSeconds <= previous;
            contiguous = contiguous && note == bank->firstNote + mapped - 1;
            previous = sample->audibleSeconds;
        }
    }

    check (ordered && contiguous && mapped == 121,
           String (mapped) + " keys, " + bank->samples.front()->name + " (" + String (bank->samples.front()->audibleSeconds, 2) + " s) down to "
               + bank->samples.back()->name + " (" + String (bank->samples.back()->audibleSeconds, 2) + " s)");

    // Whatever the file names are, and wherever the files sit.
    const auto folder = tempFolder ("rygo_mapping_test");
    writeWav (folder.getChildFile ("a_short.wav"), 48000.0, 0.2, 440.0, 440.0);
    writeWav (folder.getChildFile ("b_long.wav"), 48000.0, 2.0, 440.0, 440.0);
    writeWav (folder.getChildFile ("sub/c_mid.wav"), 48000.0, 1.0, 440.0, 440.0, 1);
    folder.getChildFile ("notes.txt").replaceWithText ("not audio");

    {
        const ScopedSamples samples (folder);
        juce::SharedResourcePointer<SampleLibrary> library;
        const auto other = library->loadBank();
        check (other->samples.size() == 3
                   && other->samples[0]->name == "b_long" && other->samples[1]->name == "c_mid" && other->samples[2]->name == "a_short",
               "files sort by length, whatever their names, and non-audio files are skipped");
    }
    folder.deleteRecursively();
}

void testPlainSampler (const BankPtr& bank)
{
    section ("3. Plain sampler: the sound is untouched");

    auto worst = 0.0f;
    for (int note : { 0, 30, 60, 90, 120 })
    {
        const auto* sample = bank->sampleForNote (note);
        Rig rig;
        const auto out = rig.render ({ { 0.0, note, 1.0f, 100.0 } }, sample->fileSeconds + 0.2);
        const auto frames = sample->trimEnd - sample->trimStart;
        const auto margin = 48 * 5;
        for (int c = 0; c < 2; ++c)
            for (int i = margin; i < frames - margin; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (c, i)
                                                     - sample->audio.getSample (juce::jmin (c, sample->audio.getNumChannels() - 1), sample->trimStart + i) * sample->normGain));
    }
    check (worst < 0.0001f, "output matches the source file sample by sample (largest difference " + String (worst, 6) + ")");

    const auto* longest = bank->samples.front().get();
    Rig rig;
    const auto out = rig.render ({ { 0.0, bank->firstNote, 1.0f, 100.0 } }, longest->fileSeconds + 0.5);
    const auto heard = lastAbove (out, 0.0005f) / rig.sampleRate;
    check (heard > longest->audibleSeconds - 0.05 && heard > 3.0,
           "the longest sample plays to its end: " + String (heard, 2) + " s (the old plugin cut everything at 3 s)");

    const auto start = firstAbove (out, 0.001f) / rig.sampleRate;
    check (start < 0.02, "trimmed silence means a note sounds right away (" + String (start * 1000.0, 1) + " ms)");
}

void testTiming()
{
    section ("4. Timing: notes land on the exact sample");

    const auto hit = 1000.0 / 48000.0;
    Rig small (48000.0, 64), large (48000.0, 1024);
    const auto a = small.render ({ { hit, 40, 1.0f, 0.3 } }, 0.5);
    const auto b = large.render ({ { hit, 40, 1.0f, 0.3 } }, 0.5);

    check (maxDifference (a, b) < 0.00001f, "buffers of 64 and 1024 samples give identical audio");

    auto silentBefore = true;
    for (int i = 0; i < 1000; ++i)
        silentBefore = silentBefore && a.getSample (0, i) == 0.0f;
    check (silentBefore && firstAbove (a, 0.0f) >= 1000, "nothing sounds before the note's sample");
}

void testFormats()
{
    section ("5. Stereo files and sample rates");

    const auto folder = tempFolder ("rygo_format_test");
    writeWav (folder.getChildFile ("stereo_44k.wav"), 44100.0, 1.0, 440.0, 660.0);

    {
        const ScopedSamples samples (folder);

        for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        {
            Rig rig (rate);
            rig.set (pid::trimSilence, 0.0f);

            const auto note = SampleLibrary::firstNoteForCount (1);
            const auto out = rig.render ({ { 0.0, note, 1.0f, 100.0 } }, 1.3);
            const auto frames = juce::roundToInt (rate * 0.5);
            const auto left = crossingsPerSecond (out, 0, juce::roundToInt (rate * 0.1), frames, rate);
            const auto right = crossingsPerSecond (out, 1, juce::roundToInt (rate * 0.1), frames, rate);
            const auto length = lastAbove (out, 0.0005f) / rate;

            check (std::abs (left - 880.0) < 6.0 && std::abs (right - 1320.0) < 6.0,
                   "at " + String ((int) rate) + " Hz the pitch stays right and left and right stay separate ("
                       + String (left, 0) + " / " + String (right, 0) + " crossings per second)");
            check (std::abs (length - 1.0) < 0.05, "at " + String ((int) rate) + " Hz the length stays right (" + String (length, 3) + " s)");
        }
    }

    folder.deleteRecursively();
}

void testModes (const BankPtr& bank)
{
    section ("6. Play modes");

    const auto note = bank->firstNote + 40;
    const auto* sample = bank->sampleForNote (note);
    const auto period = sample->trimEnd - sample->trimStart;
    const auto seconds = period / 48000.0;

    {
        Rig rig;
        rig.set (pid::mode, oneShot);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 0.05 } }, seconds * 2.0);
        const auto heard = lastAbove (out, 0.0002f) / rig.sampleRate;
        check (heard > sample->audibleSeconds * 0.9 && heard < seconds + 0.01,
               "One Shot: a 0.05 s note plays the whole " + String (seconds, 2) + " s sample once (" + String (heard, 2) + " s)");
    }

    {
        Rig rig;
        rig.set (pid::mode, oneShot);
        const auto out = rig.render ({ { 0.0, note, 1.0f, seconds * 3.0 } }, seconds * 3.5);
        check (lastAbove (out, 0.0002f) < period + 480, "One Shot: a long note doesn't repeat it");
    }

    {
        Rig rig;
        rig.set (pid::mode, gate);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 0.2 } }, seconds + 0.3);
        const auto heard = lastAbove (out, 0.0002f) / rig.sampleRate;
        check (seconds > 0.5 && heard < 0.35, "Gate: letting go ends the sample early (" + String (heard, 2) + " s)");
    }

    {
        Rig rig;
        rig.set (pid::mode, gate);
        const auto out = rig.render ({ { 0.0, note, 1.0f, seconds * 3.0 } }, seconds * 3.5);
        check (lastAbove (out, 0.0002f) < period + 480, "Gate: holding past the end leaves silence, it doesn't repeat");
    }

    {
        Rig rig;
        rig.set (pid::mode, loop);
        const auto held = seconds * 2.5;
        const auto out = rig.render ({ { 0.0, note, 1.0f, held } }, held + 0.5);

        auto worst = 0.0f;
        const auto margin = 480;
        for (int i = margin; i < period - margin; ++i)
            worst = juce::jmax (worst, std::abs (out.getSample (0, i) - out.getSample (0, i + period)));

        const auto heard = lastAbove (out, 0.0005f) / rig.sampleRate;
        check (worst < 0.001f, "Loop: the second pass starts from the very beginning and matches the first (largest difference " + String (worst, 5) + ")");
        check (rmsOf (out, period * 2 + margin, 2400) > 0.0001f && heard > held - 0.02 && heard < held + 0.2,
               "Loop: it keeps playing until the note ends, " + String (held, 2) + " s, then releases (" + String (heard, 2) + " s)");
    }

    {
        Rig rig;
        rig.set (pid::mode, loop);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 0.1 } }, seconds * 2.0);
        check (lastAbove (out, 0.0005f) / rig.sampleRate < 0.25, "Loop: a short note ends with the key, it never reaches the restart");
    }

    {
        // The seam: a restart must not click.
        Rig rig;
        rig.set (pid::mode, loop);
        const auto out = rig.render ({ { 0.0, note, 1.0f, seconds * 2.5 } }, seconds * 2.5);
        auto seamJump = 0.0f;
        for (int i = period - 960; i < period + 960; ++i)
            seamJump = juce::jmax (seamJump, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
        auto bodyJump = 0.0f;
        for (int i = 960; i < period - 960; ++i)
            bodyJump = juce::jmax (bodyJump, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
        check (seamJump <= bodyJump * 1.5f + 0.001f, "Loop: the restart is smooth (largest step " + String (seamJump, 4) + ", within the sample " + String (bodyJump, 4) + ")");
    }

    {
        Rig rig;
        rig.set (pid::mode, loop);
        rig.set (pid::reverse, 1.0f);
        const auto held = seconds * 2.5;
        const auto out = rig.render ({ { 0.0, note, 1.0f, held } }, held + 0.5);
        auto worst = 0.0f;
        for (int i = 480; i < period - 480; ++i)
            worst = juce::jmax (worst, std::abs (out.getSample (0, i) - out.getSample (0, i + period)));
        check (worst < 0.001f, "Loop works in reverse too");
    }

    {
        Rig rig;
        rig.set (pid::mode, loop);
        rig.set (pid::start, 50.0f);
        const auto held = seconds * 1.5;
        const auto out = rig.render ({ { 0.0, note, 1.0f, held } }, held + 0.3);
        const auto half = (int) (period * 0.5);
        auto worst = 0.0f;
        for (int i = 480; i < half - 480; ++i)
            worst = juce::jmax (worst, std::abs (out.getSample (0, i) - out.getSample (0, i + half)));
        check (worst < 0.001f, "Loop restarts from the START knob's position");
    }
}

void testControls (const BankPtr& bank)
{
    section ("7. Synth controls");

    const auto note = bank->firstNote + 5;

    {
        Rig rig;
        rig.set (pid::reverse, 1.0f);
        rig.set (pid::attack, 0.0f);
        const auto* sample = bank->sampleForNote (note);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 100.0 } }, sample->fileSeconds + 0.1);
        auto worst = 0.0f;
        const auto frames = sample->trimEnd - sample->trimStart;
        for (int i = 480; i < frames - 480; ++i)
            worst = juce::jmax (worst, std::abs (out.getSample (0, i) - sample->audio.getSample (0, sample->trimEnd - 1 - i) * sample->normGain));
        check (worst < 0.0001f, "reverse plays the file exactly backwards");
    }

    {
        Rig rig;
        rig.set (pid::decay, 100.0f);
        rig.set (pid::sustain, 0.0f);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 100.0 } }, 1.0);
        check (lastAbove (out, 0.001f) < rig.sampleRate * 0.15, "decay and sustain 0 end the sound in time");
    }

    {
        Rig rig;
        rig.set (pid::release, 100.0f);
        const auto out = rig.render ({ { 0.0, note, 1.0f, 0.2 } }, 1.5);
        check (lastAbove (out, 0.001f) < rig.sampleRate * 0.35, "letting go of the key releases the note");
    }

    {
        Rig open, closed;
        closed.set (pid::cutoff, 200.0f);
        const auto a = open.render (phrase(), 1.2);
        const auto b = closed.render (phrase(), 1.2);
        const auto bright = [] (const AudioBuffer<float>& x)
        {
            auto sum = 0.0;
            for (int i = 1; i < x.getNumSamples(); ++i)
                sum += std::pow (x.getSample (0, i) - x.getSample (0, i - 1), 2.0f);
            return sum;
        };
        check (bright (b) < bright (a) * 0.1, "cutoff at 200 Hz removes the highs");
    }

    {
        Rig rig;
        rig.set (pid::cutoff, 400.0f);
        rig.set (pid::resonance, 100.0f);
        rig.set (pid::filterEnv, 100.0f);
        const auto out = rig.render (phrase(), 1.2);
        check (isFinite (out) && out.getMagnitude (0, out.getNumSamples()) < 8.0f, "full resonance with envelope stays stable");
    }

    {
        Rig quiet, loud;
        quiet.set (pid::normalize, 0.0f);
        const auto* sample = bank->sampleForNote (note);
        const auto a = quiet.render ({ { 0.0, note, 1.0f, 100.0 } }, 1.0);
        const auto b = loud.render ({ { 0.0, note, 1.0f, 100.0 } }, 1.0);
        const auto ratio = rmsOf (b, 0, 48000) / juce::jmax (0.000001f, rmsOf (a, 0, 48000));
        check (std::abs (ratio - sample->normGain) < 0.01f, "normalize changes the level by exactly the sample's own gain");
    }

    {
        Rig rig;
        rig.set (pid::gain, -60.0f);
        rig.startPlayback();
        const auto out = rig.render (phrase(), 1.0);
        check (out.getMagnitude (0, out.getNumSamples()) < 0.0001f, "gain at the bottom of its range is silence");
    }
}

void testEffects()
{
    section ("8. Effects");

    Rig dryRig;
    const auto dry = dryRig.render (phrase(), 2.0);

    struct Setting { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Setting> settings
    {
        { "Dirt",    { { pid::dirtOn, 1 } } },
        { "Stutter", { { pid::stutterOn, 1 }, { pid::stutterChance, 100 } } },
        { "Echo",    { { pid::echoOn, 1 } } },
        { "Reverb",  { { pid::reverbOn, 1 } } },
        { "Chaos",   { { pid::chaosOn, 1 } } },
    };

    for (const auto& setting : settings)
    {
        Rig rig;
        for (const auto& [id, value] : setting.values)
            rig.set (id, value);

        const auto out = rig.render (phrase(), 2.0);
        const auto difference = maxDifference (out, dry);
        const auto peak = out.getMagnitude (0, out.getNumSamples());
        check (difference > 0.01f && isFinite (out) && peak < 4.0f,
               String (setting.name) + " changes the sound and stays stable (difference " + String (difference, 3) + ", peak " + String (peak, 2) + ")");

        Rig bypassed;
        for (const auto& [id, value] : setting.values)
            bypassed.set (id, value);
        bypassed.set (pid::fxOn, 0.0f);
        bypassed.startPlayback();
        const auto clean = bypassed.render (phrase(), 2.0);
        check (maxDifference (clean, dry) < 0.00001f, String (setting.name) + " is gone when the FX switch is off: a plain sampler again");
    }

    {
        Rig rig;
        rig.set (pid::echoOn, 1.0f);
        rig.set (pid::echoMix, 100.0f);
        const auto out = rig.render ({ { 0.0, 60, 1.0f, 0.1 } }, 3.0);
        auto widest = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            widest = juce::jmax (widest, std::abs (out.getSample (0, i) - out.getSample (1, i)));
        check (widest > 0.01f, "echo makes dual-mono samples stereo");
    }

    {
        Rig rig;
        rig.set (pid::chaosOn, 1.0f);
        rig.set (pid::chaosPitch, 0.0f);
        rig.set (pid::chaosReverse, 0.0f);
        rig.set (pid::chaosPan, 100.0f);
        const auto out = rig.render ({ { 0.0, 60, 1.0f, 0.2 } }, 0.3);
        check (std::abs (rmsOf (out, 0, 14400) - juce::jmin (out.getRMSLevel (0, 0, 14400), out.getRMSLevel (1, 0, 14400))) > 0.0001f,
               "chaos pans a note away from the centre");
    }

    {
        // The old plugin wiped the reverb tail whenever a filter or reverb knob moved.
        Rig rig;
        rig.set (pid::reverbOn, 1.0f);
        rig.set (pid::reverbMix, 100.0f);
        rig.set (pid::reverbSize, 90.0f);
        const auto moveAt = juce::roundToInt (rig.sampleRate * 0.5);
        const auto out = rig.render ({ { 0.0, 60, 1.0f, 0.1 } }, 1.0, [&] (int start)
        {
            if (start >= moveAt)
            {
                rig.set (pid::cutoff, 3000.0f);
                rig.set (pid::reverbSize, 70.0f);
            }
        });
        const auto before = rmsOf (out, moveAt - 4800, 4800);
        const auto after = rmsOf (out, moveAt, 4800);
        check (before > 0.0001f && after > before * 0.4f, "moving knobs keeps the reverb tail ("
               + String (before, 4) + " before, " + String (after, 4) + " after)");
    }
}

void testRootDetection (const BankPtr& bank)
{
    section ("9. Root note detection");

    const auto detect = [] (double hz)
    {
        const auto rate = 48000.0;
        AudioBuffer<float> audio (1, 48000);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return estimatePitch (audio, rate, 0, audio.getNumSamples());
    };

    for (const auto note : { 40, 52, 60, 65, 69, 81 })
    {
        const auto hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        const auto found = detect (hz);
        const auto foundNote = found.hz > 0.0 ? juce::roundToInt (69.0 + 12.0 * std::log2 (found.hz / 440.0)) : -1;
        check (foundNote == note && found.isClear, "a sine at " + String (hz, 1) + " Hz reads as "
               + juce::MidiMessage::getMidiNoteName (foundNote, true, true, 3) + " (" + String (found.hz, 1) + " Hz)");
    }

    {
        // A rich sound whose fundamental is weaker than its overtones: octave errors would show here.
        const auto rate = 48000.0;
        const auto hz = 220.0;
        AudioBuffer<float> audio (1, 48000);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            auto value = 0.0;
            for (int h = 1; h <= 8; ++h)
                value += (h == 1 ? 0.3 : 0.6 / h) * std::sin (juce::MathConstants<double>::twoPi * hz * h * i / rate);
            audio.setSample (0, i, (float) value * 0.4f);
        }
        const auto found = estimatePitch (audio, rate, 0, audio.getNumSamples());
        check (std::abs (found.hz - hz) < 4.0 && found.isClear, "overtones don't fool it: 220 Hz with a weak fundamental reads as " + String (found.hz, 1) + " Hz");
    }

    {
        juce::Random random (7);
        AudioBuffer<float> audio (1, 48000);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample (0, i, (random.nextFloat() - 0.5f) * 0.5f);
        check (! estimatePitch (audio, 48000.0, 0, audio.getNumSamples()).isClear, "white noise has no clear pitch");

        // Noise with a band: there's no period to find, but the ear still hears a note in it.
        const auto rate = 48000.0;
        const auto centre = 349.23;
        AudioBuffer<float> band (1, 48000);
        auto low = 0.0f, bandPass = 0.0f;
        const auto f = 2.0f * std::sin (juce::MathConstants<float>::pi * (float) centre / (float) rate);
        for (int i = 0; i < band.getNumSamples(); ++i)
        {
            const auto input = (random.nextFloat() - 0.5f) * 2.0f;
            low += f * bandPass;
            const auto high = input - low - 0.03f * bandPass;
            bandPass += f * high;
            band.setSample (0, i, bandPass * 0.05f);
        }
        const auto bandEstimate = estimatePitch (band, rate, 0, band.getNumSamples());
        const auto bandNote = bandEstimate.hz > 0.0 ? juce::roundToInt (69.0 + 12.0 * std::log2 (bandEstimate.hz / 440.0)) : -1;
        check (std::abs (bandNote - 65) <= 1, "noise ringing around F3 is placed near F3: "
               + juce::MidiMessage::getMidiNoteName (bandNote, true, true, 3) + (bandEstimate.isClear ? ", read as a clear pitch" : ", as an estimate"));
    }

    {
        AudioBuffer<float> silence (1, 48000);
        silence.clear();
        check (estimatePitch (silence, 48000.0, 0, 48000).hz <= 0.0 && estimatePitch (silence, 48000.0, 0, 20).hz <= 0.0,
               "silence and very short files are handled");
    }

    int clear = 0;
    std::map<int, int> octaves, notes;
    for (const auto& sample : bank->samples)
    {
        if (sample->hasClearPitch)
            ++clear;
        ++octaves[sample->rootNote / 12 - 2];
        ++notes[sample->rootNote];
    }

    String spread;
    for (const auto& [octave, count] : octaves)
        spread << "C" << octave << ": " << count << "  ";

    std::cout << "        " << clear << " of " << (int) bank->samples.size() << " samples have a clear pitch, the rest are estimates." << std::endl;
    std::cout << "        roots by octave: " << spread << " (" << (int) notes.size() << " different notes)" << std::endl;
    check (notes.size() > 12, "roots really differ from sample to sample: " + String ((int) notes.size()) + " different notes");
}

void testMusicMode (const BankPtr& bank)
{
    section ("10. Music mode");

    const auto folder = tempFolder ("rygo_music_test");
    writeWav (folder.getChildFile ("tone.wav"), 48000.0, 2.0, 440.0, 440.0);

    {
        const ScopedSamples samples (folder);

        const auto toneSetup = [] (Rig& rig)
        {
            rig.set (pid::musicOn, 1.0f);
            rig.set (pid::musicLock, 1.0f);
            rig.render ({}, 0.01);
        };

        {
            Rig rig;
            rig.render ({}, 0.01);
            const auto loaded = rig.proc.getLoadedBank();
            const auto* tone = loaded->sampleForNote (loaded->firstNote);
            check (tone != nullptr && tone->rootNote == 69 && tone->hasClearPitch, "a 440 Hz sample gets A3 as its root");
        }

        {
            Rig rig;
            toneSetup (rig);
            const auto out = rig.render ({ { 0.0, 69, 1.0f, 100.0 } }, 2.2);
            const auto* tone = rig.proc.getLoadedBank()->samples[0].get();
            auto worst = 0.0f;
            for (int i = 480; i < tone->trimEnd - tone->trimStart - 480; ++i)
                worst = juce::jmax (worst, std::abs (out.getSample (0, i) - tone->audio.getSample (0, tone->trimStart + i) * tone->normGain));
            check (worst < 0.0001f, "the root key plays the sample exactly as it is (largest difference " + String (worst, 6) + ")");
        }

        const auto measure = [&] (Rig& rig, int key, double seconds)
        {
            const auto out = rig.render ({ { 0.0, key, 1.0f, 100.0 } }, seconds);
            struct Result { double crossings, length; float level; };
            return Result { crossingsPerSecond (out, 0, 12000, 24000, 48000.0) / 2.0,
                            lastAbove (out, 0.001f) / 48000.0,
                            rmsOf (out, 12000, 24000) };
        };

        Rig rootRig;
        toneSetup (rootRig);
        const auto rootLevel = measure (rootRig, 69, 2.2).level;

        for (const auto key : { 57, 45, 76, 81, 93 })
        {
            Rig rig;
            toneSetup (rig);
            const auto r = measure (rig, key, 2.2);
            const auto expected = 440.0 * std::pow (2.0, (key - 69) / 12.0);
            const auto db = juce::Decibels::gainToDecibels (r.level / rootLevel);

            check (std::abs (r.crossings - expected) < expected * 0.02,
                   "keep length: " + juce::MidiMessage::getMidiNoteName (key, true, true, 3) + " plays " + String (r.crossings, 1) + " Hz (expected " + String (expected, 1) + ")");
            check (std::abs (r.length - 2.0) < 0.1, "keep length: the sample stays 2 s (" + String (r.length, 2) + " s)");
            check (std::abs (db) < 3.0f, "keep length: the level stays even (" + String (db, 1) + " dB)");
        }

        for (const auto [key, expectedLength] : { std::pair { 57, 4.0 }, std::pair { 81, 1.0 } })
        {
            Rig rig;
            toneSetup (rig);
            rig.set (pid::musicKeepLength, 0.0f);
            const auto r = measure (rig, key, 4.4);
            const auto expected = 440.0 * std::pow (2.0, (key - 69) / 12.0);
            check (std::abs (r.crossings - expected) < expected * 0.01 && std::abs (r.length - expectedLength) < 0.05,
                   "keep length off: " + juce::MidiMessage::getMidiNoteName (key, true, true, 3) + " plays " + String (r.crossings, 1)
                       + " Hz for " + String (r.length, 2) + " s, like a tape");
        }

        {
            Rig rig;
            toneSetup (rig);
            rig.proc.setRootOverride (57);
            const auto r = measure (rig, 57, 2.2);
            check (std::abs (r.crossings - 440.0) < 4.0, "a root set by hand moves the keyboard: A2 now plays the sample unchanged");
        }

        {
            Rig rig;
            rig.set (pid::musicLock, 1.0f);
            const auto out = rig.render ({ { 0.0, 80, 1.0f, 0.2 } }, 0.3);
            check (out.getMagnitude (0, out.getNumSamples()) == 0.0f, "with Music off, a closed lock does nothing: unmapped keys stay silent");
        }
    }

    folder.deleteRecursively();

    // The real samples.
    {
        Rig rig;
        rig.set (pid::musicOn, 1.0f);
        rig.render ({ { 0.0, 40, 1.0f, 0.1 } }, 0.2);
        check (rig.proc.getSelectedNote() == 40, "while unlocked, the selection follows the last note played");

        const auto normal = rig.render ({ { 0.0, 80, 1.0f, 0.1 } }, 0.2);
        check (rig.proc.getSelectedNote() == 80 && normal.getMagnitude (0, normal.getNumSamples()) > 0.0f, "and everything plays as usual");

        rig.proc.selectNote (50);
        check (rig.proc.getSelectedNote() == 50, "a sample can be picked from the list");

        rig.set (pid::musicLock, 1.0f);
        rig.render ({ { 0.0, 90, 1.0f, 0.1 } }, 0.2);
        check (rig.proc.getSelectedNote() == 50, "while locked, playing doesn't change the selection");

        rig.proc.selectNote (70);
        check (rig.proc.getSelectedNote() == 50, "and neither does the list");

        rig.set (pid::musicLock, 0.0f);
        rig.render ({ { 0.0, 77, 1.0f, 0.1 } }, 0.2);
        check (rig.proc.getSelectedNote() == 77, "opening the lock goes back to choosing");
    }

    {
        Rig plain, locked;
        const auto chosen = bank->firstNote + 30;
        const auto* sample = bank->sampleForNote (chosen);

        locked.set (pid::musicOn, 1.0f);
        locked.proc.selectNote (chosen);
        locked.set (pid::musicLock, 1.0f);

        const auto root = sample->rootNote;
        const auto out = locked.render ({ { 0.0, juce::jlimit (0, 127, root + 7), 1.0f, 100.0 } }, sample->fileSeconds + 0.3);
        const auto heard = lastAbove (out, 0.0005f) / locked.sampleRate;
        check (std::abs (heard - sample->audibleSeconds) < sample->audibleSeconds * 0.1 + 0.05,
               "a real sample keeps its length when pitched up a fifth (" + String (heard, 2) + " s, sample " + String (sample->audibleSeconds, 2) + " s)");
        check (isFinite (out), "and stays clean");
    }

    {
        Rig rig;
        rig.set (pid::musicOn, 1.0f);
        rig.set (pid::musicLock, 1.0f);
        const auto out = rig.render ({ { 0.0, 60, 1.0f, 1.0 }, { 0.0, 64, 1.0f, 1.0 }, { 0.0, 67, 1.0f, 1.0 } }, 1.2);
        check (isFinite (out) && rmsOf (out, 4800, 24000) > 0.0001f && out.getMagnitude (0, out.getNumSamples()) < 4.0f, "chords work");
    }

    for (const auto mode : { oneShot, gate, loop })
    {
        Rig rig;
        rig.set (pid::musicOn, 1.0f);
        rig.proc.selectNote (bank->firstNote + 60);
        rig.set (pid::musicLock, 1.0f);
        rig.set (pid::mode, (float) mode);
        rig.set (pid::reverse, 1.0f);
        const auto* sample = bank->sampleForNote (bank->firstNote + 60);
        const auto key = juce::jlimit (0, 127, sample->rootNote + 5);
        const auto out = rig.render ({ { 0.0, key, 1.0f, sample->fileSeconds * 2.5 } }, sample->fileSeconds * 3.0);
        check (isFinite (out) && out.getMagnitude (0, out.getNumSamples()) > 0.001f,
               "a locked sample plays in reverse in " + String (mode == oneShot ? "One Shot" : mode == gate ? "Gate" : "Loop"));
    }

    {
        Rig rig;
        rig.set (pid::musicOn, 1.0f);
        rig.proc.selectNote (55);
        rig.proc.setRootOverride (62);
        rig.set (pid::musicLock, 1.0f);
        rig.proc.applyPreset (3);
        check (rig.get (pid::musicOn) > 0.5f && rig.get (pid::musicLock) > 0.5f && rig.proc.getSelectedNote() == 55,
               "changing presets leaves the Music section alone");

        juce::MemoryBlock saved;
        rig.proc.getStateInformation (saved);
        const auto before = rig.render ({ { 0.0, 70, 1.0f, 0.5 } }, 0.7);

        Rig reopened;
        reopened.proc.setStateInformation (saved.getData(), (int) saved.getSize());
        reopened.startPlayback();
        check (reopened.get (pid::musicLock) > 0.5f && reopened.proc.getSelectedNote() == 55 && reopened.proc.getRootOverride() == 62,
               "the lock, the chosen sample and the root come back with the project");
        const auto after = reopened.render ({ { 0.0, 70, 1.0f, 0.5 } }, 0.7);
        check (after.getMagnitude (0, after.getNumSamples()) > 0.001f && isFinite (after), "and the sample plays right away after reopening");
    }

    {
        // The Sample Lock with grains, through the same stress as everything else.
        Rig rig;
        rig.set (pid::musicOn, 1.0f);
        rig.set (pid::musicLock, 1.0f);
        rig.set (pid::dirtOn, 1.0f);
        rig.set (pid::stutterOn, 1.0f);
        rig.set (pid::echoOn, 1.0f);
        rig.set (pid::reverbOn, 1.0f);
        rig.set (pid::chaosOn, 1.0f);
        rig.render (phrase(), 0.5);   // gets every effect past its first use

        allocationCount = 0;
        const auto blockSize = 512;
        AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.ensureSize (2048);
        int total = 0;

        for (int block = 0; block < 400; ++block)
        {
            midi.clear();
            if (block % 3 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 30 + block % 70, (juce::uint8) 100), block % blockSize);
            if (block % 5 == 0)
                midi.addEvent (juce::MidiMessage::noteOff (1, 30 + (block + 50) % 70), 10);

            countAllocations = true;
            rig.proc.processBlock (buffer, midi);
            countAllocations = false;
            total += allocationCount;
            allocationCount = 0;
        }
        check (total == 0, "no memory allocated on the audio thread in 400 busy blocks, locked with all effects (" + String (total) + ")");
    }
}

void testVoices (const BankPtr& bank)
{
    section ("11. Polyphony and stress");

    Rig rig (48000.0, 128);
    std::vector<Note> flood;
    for (int i = 0; i < 400; ++i)
        flood.push_back ({ i * 0.004, bank->firstNote + i % 100, 1.0f, 2.0 });

    const auto out = rig.render (flood, 3.0);
    check (isFinite (out), "400 overlapping notes stay finite");

    {
        Rig rig2;
        const auto a = rig2.render ({ { 0.0, 60, 1.0f, 2.0 }, { 0.1, 60, 1.0f, 2.0 }, { 0.2, 60, 1.0f, 2.0 } }, 0.4);
        Rig rig3;
        const auto b = rig3.render ({ { 0.2, 60, 1.0f, 2.0 } }, 0.4);
        const auto later = 48000 * 2 / 10 + 960;
        check (rmsOf (a, later, 4800) < rmsOf (b, later, 4800) * 1.2f, "repeating a key replaces the previous hit instead of stacking it");
    }
}

void testState()
{
    section ("12. Saving and reopening a project");

    juce::MemoryBlock saved;
    AudioBuffer<float> before;
    {
        Rig rig;
        rig.set (pid::attack, 25.0f);
        rig.set (pid::cutoff, 1234.0f);
        rig.set (pid::reverbOn, 1.0f);
        rig.set (pid::stutterRate, 5.0f);
        rig.set (pid::mode, (float) loop);
        rig.startPlayback();
        before = rig.render (phrase(), 1.0);
        rig.proc.getStateInformation (saved);
    }

    Rig rig;
    rig.proc.setStateInformation (saved.getData(), (int) saved.getSize());
    rig.startPlayback();

    check (std::abs (rig.get (pid::attack) - 25.0f) < 0.01f && std::abs (rig.get (pid::cutoff) - 1234.0f) < 0.5f
               && rig.get (pid::reverbOn) > 0.5f && (int) rig.get (pid::stutterRate) == 5 && (int) rig.get (pid::mode) == loop,
           "every setting comes back");

    const auto after = rig.render (phrase(), 1.0);
    check (maxDifference (before, after) < 0.0001f, "the restored project sounds identical, even in an offline render");

    {
        const ScopedSamples samples (juce::File ("/this/folder/does/not/exist"));
        Rig missing;
        const auto out = missing.render (phrase(), 0.3);
        const auto bank = missing.proc.getLoadedBank();
        check (bank != nullptr && bank->folderMissing && out.getMagnitude (0, out.getNumSamples()) == 0.0f,
               "missing samples are reported, with no made-up sine waves");
    }

    {
        Rig bad;
        const char junk[] { 1, 2, 3, 4, 5 };
        bad.proc.setStateInformation (junk, sizeof (junk));
        bad.proc.setStateInformation (nullptr, 0);
        check (isFinite (bad.render (phrase(), 0.3)), "a damaged state is ignored without crashing");
    }

    {
        Rig preset;
        preset.proc.applyPreset (1);
        juce::MemoryBlock data;
        preset.set (pid::cutoff, 555.0f);
        preset.proc.getStateInformation (data);

        Rig other;
        other.proc.setStateInformation (data.getData(), (int) data.getSize());
        other.proc.setCurrentProgram (other.proc.getCurrentProgram());
        check (std::abs (other.get (pid::cutoff) - 555.0f) < 0.5f, "a host re-selecting the current program doesn't wipe restored settings");
    }
}

void testPresets()
{
    section ("13. Presets");

    Rig cleanRig;
    const auto clean = cleanRig.render (phrase(), 1.5);

    for (int i = 0; i < cleanRig.proc.getNumPrograms(); ++i)
    {
        Rig rig;
        rig.proc.applyPreset (i);
        const auto out = rig.render (phrase(), 1.5);
        const auto difference = maxDifference (out, clean);
        const auto peak = out.getMagnitude (0, out.getNumSamples());
        check (isFinite (out) && peak < 4.0f && (i == 0 ? difference < 0.00001f : difference > 0.01f),
               rig.proc.getProgramName (i) + ": peak " + String (peak, 2));
    }
}

void testEnglish()
{
    section ("14. Everything is in English");

    Rig rig;
    auto parametersOk = true;
    for (auto* parameter : rig.proc.getParameters())
        parametersOk = parametersOk && isAscii (parameter->getName (64));
    check (parametersOk, "parameter names, as a host shows them");

    auto presetsOk = true;
    for (int i = 0; i < rig.proc.getNumPrograms(); ++i)
        presetsOk = presetsOk && isAscii (rig.proc.getProgramName (i));
    check (presetsOk, "preset names");

    auto artOk = true;
    int artItems = 0;
    for (const auto& group : ui::getArtManifest())
    {
        for (const auto& item : group.items)
        {
            ++artItems;
            const auto file = String (item.file);
            const auto hasLettering = file.startsWith ("label_") || file.startsWith ("title_") || file.startsWith ("opt_")
                                   || file.startsWith ("hint_") || file.startsWith ("logo_");
            if (hasLettering && ! isAscii (item.letter))
            {
                artOk = false;
                std::cout << "        not English: " << file << std::endl;
            }
        }
    }
    check (artOk, String (artItems) + " art items: the lettering placeholders");
}

juce::Component* findById (juce::Component* root, const String& id)
{
    if (root->getComponentID() == id)
        return root;
    for (auto* child : root->getChildren())
        if (auto* found = findById (child, id))
            return found;
    return nullptr;
}

juce::Label* findLabel (juce::Slider& slider)
{
    for (auto* child : slider.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
            return label;
    return nullptr;
}

void testInterface()
{
    section ("15. Interface");

    Rig rig;
    rig.render (phrase(), 0.1);
    std::unique_ptr<juce::AudioProcessorEditor> editor (rig.proc.createEditor());

    const auto type = [&] (const char* id, const String& text) -> float
    {
        auto* knob = dynamic_cast<ui::Knob*> (findById (editor.get(), id));
        if (knob == nullptr)
            return -99999.0f;
        auto* label = findLabel (knob->slider);
        if (label == nullptr || ! label->isEditable())
            return -88888.0f;
        label->showEditor();
        if (auto* textEditor = label->getCurrentTextEditor())
            textEditor->setText (text, false);
        label->hideEditor (false);
        return rig.get (id);
    };

    check (std::abs (type (pid::release, "250 ms") - 250.0f) < 0.5f, "release: \"250 ms\"");
    check (std::abs (type (pid::release, ru ("1,5 с")) - 1500.0f) < 0.5f, ru ("release: \"1,5 с\", with a Russian comma and unit"));
    check (std::abs (type (pid::release, "2 s") - 2000.0f) < 0.5f, "release: \"2 s\"");
    check (std::abs (type (pid::attack, "12") - 12.0f) < 0.1f, "attack: a bare number");
    check (std::abs (type (pid::cutoff, ru ("2 кГц")) - 2000.0f) < 1.0f, ru ("cutoff: \"2 кГц\""));
    check (std::abs (type (pid::cutoff, "440") - 440.0f) < 1.0f, "cutoff: \"440\"");
    check (std::abs (type (pid::pitch, ru ("−3,5")) - (-3.5f)) < 0.01f, ru ("pitch: \"−3,5\", with a typographic minus"));
    check (std::abs (type (pid::gain, "-6 dB") - (-6.0f)) < 0.05f, "gain: \"-6 dB\"");
    check (std::abs (type (pid::sustain, "55%") - 55.0f) < 0.5f, "sustain: \"55%\"");
    check ((int) type (pid::stutterRate, "1/32") == 5, "stutter rate: \"1/32\"");
    check (std::abs (type (pid::sustain, "1000") - 100.0f) < 0.01f, "out-of-range values are limited to the range");

    auto knobs = 0;
    for (const auto* id : { pid::attack, pid::decay, pid::sustain, pid::release, pid::velocity, pid::cutoff, pid::resonance, pid::filterEnv,
                            pid::pitch, pid::start, pid::gain, pid::chaosPitch, pid::chaosReverse, pid::chaosPan, pid::stutterRate,
                            pid::stutterChance, pid::dirtDrive, pid::dirtBits, pid::dirtRate, pid::echoTime, pid::echoFeedback, pid::echoMix,
                            pid::reverbMix, pid::reverbSize, pid::reverbDamp })
    {
        auto* knob = dynamic_cast<ui::Knob*> (findById (editor.get(), id));
        if (knob != nullptr && knob->slider.isDoubleClickReturnEnabled() && knob->slider.getSliderStyle() == juce::Slider::RotaryVerticalDrag)
            ++knobs;
    }
    check (knobs == 25, "all 25 knobs are on the panel, with mouse drag and double-click reset");

    {
        const auto before = rig.get (pid::cutoff);
        auto* knob = dynamic_cast<ui::Knob*> (findById (editor.get(), pid::cutoff));
        knob->slider.setValue (5000.0, juce::sendNotificationSync);
        check (! juce::approximatelyEqual (before, rig.get (pid::cutoff)) && std::abs (rig.get (pid::cutoff) - 5000.0f) < 1.0f, "moving a knob changes the parameter");

        auto* parameter = rig.proc.getState().getParameter (pid::cutoff);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (700.0f));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (std::abs (knob->slider.getValue() - 700.0) < 1.0, "host automation moves the knob");
    }

    {
        auto* toggle = dynamic_cast<juce::Button*> (findById (editor.get(), pid::reverbOn));
        check (toggle != nullptr, "the reverb on/off switch is on the panel");
        if (toggle != nullptr)
        {
            toggle->setToggleState (true, juce::sendNotificationSync);
            check (rig.get (pid::reverbOn) > 0.5f, "the switch controls its effect");
        }
    }

    {
        auto* musicSwitch = dynamic_cast<juce::Button*> (findById (editor.get(), pid::musicOn));
        auto* lock = dynamic_cast<juce::Button*> (findById (editor.get(), pid::musicLock));
        auto* sampleBox = dynamic_cast<juce::ComboBox*> (findById (editor.get(), "sampleBox"));
        auto* rootBox = dynamic_cast<juce::ComboBox*> (findById (editor.get(), "rootBox"));

        check (musicSwitch != nullptr && lock != nullptr && sampleBox != nullptr && rootBox != nullptr, "the Music panel has its switch, lock and lists");

        if (musicSwitch != nullptr && lock != nullptr && sampleBox != nullptr && rootBox != nullptr)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (! lock->isEnabled() && ! sampleBox->isEnabled(), "with Music off, the lock and the lists can't be used");

            musicSwitch->setToggleState (true, juce::sendNotificationSync);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (lock->isEnabled() && sampleBox->isEnabled() && rootBox->isEnabled(), "the switch makes them available");
            check (sampleBox->getNumItems() == 121, "the list offers all " + String (sampleBox->getNumItems()) + " samples");

            rig.render ({ { 0.0, 56, 1.0f, 0.1 } }, 0.2);   // G#2
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (sampleBox->getText() == "G#2", "the label shows the last sample played: " + sampleBox->getText());

            sampleBox->setSelectedId (40 + 1, juce::sendNotificationSync);
            check (rig.proc.getSelectedNote() == 40, "picking from the list chooses that sample");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (sampleBox->getText() == "E1", "and the label follows: " + sampleBox->getText());

            lock->setToggleState (true, juce::sendNotificationSync);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (rig.proc.isSampleLocked() && ! sampleBox->isEnabled(), "closing the lock freezes the choice");

            rootBox->setSelectedId (62 + 1, juce::sendNotificationSync);
            check (rig.proc.getRootOverride() == 62, "the root can be set from its list");
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (rootBox->getText() == "D3", "and shows the note: " + rootBox->getText());

            lock->setToggleState (false, juce::sendNotificationSync);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            check (! rig.proc.isSampleLocked() && sampleBox->isEnabled(), "opening the lock brings the choice back");
        }
    }
}

void testPerformance (const BankPtr& bank)
{
    section ("16. Performance");

    const auto run = [&] (bool locked)
    {
        Rig rig (48000.0, 512);
        rig.set (pid::dirtOn, 1.0f);
        rig.set (pid::stutterOn, 1.0f);
        rig.set (pid::echoOn, 1.0f);
        rig.set (pid::reverbOn, 1.0f);
        rig.set (pid::chaosOn, 1.0f);
        rig.set (pid::cutoff, 3000.0f);
        rig.set (pid::mode, (float) oneShot);

        if (locked)
        {
            // The longest sample, so every voice keeps running to the end of the measurement.
            rig.set (pid::musicOn, 1.0f);
            rig.proc.selectNote (bank->firstNote);
            rig.set (pid::musicLock, 1.0f);
            rig.set (pid::chaosPitch, 0.0f);
        }

        std::vector<Note> notes;
        for (int i = 0; i < 32; ++i)
            notes.push_back ({ 0.0, 36 + i, 1.0f, 5.0 });

        const auto seconds = 5.0;
        const auto startTime = std::chrono::steady_clock::now();
        const auto out = rig.render (notes, seconds);
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - startTime).count();
        check (isFinite (out), "the output is clean");
        return elapsed / seconds * 100.0;
    };

    const auto normal = run (false);
    const auto locked = run (true);
    std::cout << "        32 voices with filter and all effects: " << String (normal, 1) << "% of one core" << std::endl;
    std::cout << "        the same, locked, pitch shifted, keeping length: " << String (locked, 1) << "% of one core" << std::endl;
    check (locked < 50.0, "the heaviest case runs well faster than real time");

    {
        juce::SharedResourcePointer<SampleLibrary> library;
        const auto startTime = std::chrono::steady_clock::now();
        const auto fresh = estimatePitch (bank->samples.front()->audio, bank->samples.front()->sourceRate, 0, bank->samples.front()->audio.getNumSamples());
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - startTime).count();
        std::cout << "        root detection for the longest sample: " << String (elapsed * 1000.0, 1) << " ms (" << String (fresh.hz, 1) << " Hz)" << std::endl;
    }
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;

    const juce::StringArray args (argv + 1, argc - 1);

    if (args[0] == "--make-art" && args.size() >= 3)
        return tests::makeArt (juce::File (args[1]), juce::File (args[2]));

    if (args[0] == "--roots")
    {
        juce::SharedResourcePointer<SampleLibrary> library;
        const auto bank = library->loadBank();
        for (int note = 0; note < 128; ++note)
            if (const auto* sample = bank->sampleForNote (note))
                std::cout << juce::MidiMessage::getMidiNoteName (note, true, true, 3) << "  " << sample->name << "  root "
                          << juce::MidiMessage::getMidiNoteName (sample->rootNote, true, true, 3)
                          << (sample->hasClearPitch ? "  clear" : "  estimate") << std::endl;
        return 0;
    }

    if (args[0] == "--snapshot" && args.size() >= 2)
        return tests::snapshot (juce::File (args[1]), args[2] == "locked");

    // Tests must not touch the real settings file.
    const auto settings = tempFolder ("rygo_test_settings");
    setenv ("RYGO_SETTINGS_DIR", settings.getFullPathName().toRawUTF8(), 1);

    int result = 1;
    {
        juce::SharedResourcePointer<SampleLibrary> library;
        const auto bank = library->loadBank();   // kept alive so the decoded audio stays cached

        testSamples (bank);
        testMapping (bank);
        testPlainSampler (bank);
        testTiming();
        testFormats();
        testModes (bank);
        testControls (bank);
        testEffects();
        testRootDetection (bank);
        testMusicMode (bank);
        testVoices (bank);
        testState();
        testPresets();
        testEnglish();
        testInterface();
        testPerformance (bank);

        std::cout << "\n" << passed << " passed, " << failed << " failed" << std::endl;
        result = failed == 0 ? 0 : 1;
    }

    settings.deleteRecursively();
    return result;
}
