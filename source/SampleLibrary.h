#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace rygo
{
/** Cyrillic text in sources is UTF-8; juce::String needs to be told so explicitly. */
inline juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }

struct SampleData
{
    static constexpr int overviewBins = 400;
    static constexpr int defaultRootNote = 60;

    juce::String name;
    juce::AudioBuffer<float> audio;
    double sourceRate = 44100.0;
    double fileSeconds = 0.0;
    double audibleSeconds = 0.0;   // what the ear hears: first to last frame above the silence threshold
    int trimStart = 0;
    int trimEnd = 0;
    float peak = 0.0f;
    float normGain = 1.0f;
    int rootNote = defaultRootNote;   // the MIDI note this sample sounds closest to
    bool hasClearPitch = false;       // false: noise or similar, and rootNote is an estimate from its spectrum
    std::vector<float> overview;      // peak per bin, for drawing the waveform
};

using SamplePtr = std::shared_ptr<const SampleData>;

/** All the samples mapped onto the keyboard: the lowest key gets the longest one. */
struct Bank
{
    std::vector<SamplePtr> samples;   // sorted longest to shortest
    std::array<int, 128> noteToSample {};
    int firstNote = 0;
    bool folderMissing = false;
    juce::StringArray notes;          // anything the user should know about this bank

    const SampleData* sampleForNote (int note) const noexcept
    {
        if (! juce::isPositiveAndBelow (note, 128) || noteToSample[(size_t) note] < 0)
            return nullptr;
        return samples[(size_t) noteToSample[(size_t) note]].get();
    }
};

using BankPtr = std::shared_ptr<const Bank>;

/** The note a sound is closest to. Tuned sounds have a real fundamental; for noisy ones
    (and most of the factory samples are) the frequency is the strongest part of the spectrum,
    which is what the ear latches on to. Hz is 0 only for silence or a sound too short to measure.
*/
struct PitchEstimate
{
    double hz = 0.0;
    bool isClear = false;
};

PitchEstimate estimatePitch (const juce::AudioBuffer<float>& audio, double sampleRate, int firstFrame, int lastFrame);

/** Finds, decodes and analyses the samples shipped inside the plugin. One instance is shared
    per process, so several plugin instances share one copy of the audio.
*/
class SampleLibrary
{
public:
    static constexpr int maxKeys = 128;
    static constexpr double maxSecondsPerFile = 300.0;

    SampleLibrary();

    static int firstNoteForCount (int count);

    juce::File getFactoryRoot() const;

    /** Blocking: call from a background thread. Returns nullptr if cancelled. */
    BankPtr loadBank (const std::function<bool()>& shouldCancel = {});

    double getUiScale();
    void setUiScale (double scale);

private:
    SamplePtr loadSample (const juce::File& file, juce::StringArray& notes);
    std::vector<juce::File> findAudioFiles (const juce::File& folder) const;
    juce::PropertiesFile& settings();

    juce::AudioFormatManager formats;
    juce::CriticalSection lock;
    std::map<juce::String, std::weak_ptr<const SampleData>> cache;
    std::unique_ptr<juce::PropertiesFile> settingsFile;

    JUCE_DECLARE_NON_COPYABLE (SampleLibrary)
};
}
