#pragma once

#include "SampleLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace rygo
{
/** Polyphonic playback of the samples. Everything here runs on the audio thread and never allocates. */
class SamplerEngine
{
public:
    static constexpr int voicePool = 48;   // more slots than the limit, so stolen voices can fade out
    static constexpr int polyLimit = 32;

    /** In the order of the Mode parameter's choices. */
    enum class PlayMode
    {
        oneShot,   // plays to its end whatever the key does
        gate,      // plays while the key is held
        loop       // plays to its end and starts again while the key is held
    };

    struct Params
    {
        float attackSec = 0.001f, decaySec = 0.5f, sustain = 1.0f, releaseSec = 0.06f, velocity = 1.0f;
        int filterType = 0;
        float cutoffHz = 20000.0f, resonance = 0.0f, filterEnv = 0.0f;
        float pitchSt = 0.0f, startFrac = 0.0f;
        PlayMode mode = PlayMode::gate;
        bool reverse = false, trim = true, normalize = true;
        bool chaosOn = false;
        float chaosPitch = 0.0f, chaosReverse = 0.0f, chaosPan = 0.0f;
        bool locked = false;       // Music mode with the Sample Lock closed: one sample, pitched across the keyboard
        bool keepLength = true;
    };

    SamplerEngine();

    void prepare (double sampleRate);
    void reset();

    /** Called once, while no voice can be playing the old (empty) bank. */
    void setBank (BankPtr newBank) noexcept;

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Params& params);

    /** The root note in effect for a sample, given a manual override (negative: none). */
    static int effectiveRoot (const SampleData& sample, int rootOverride) noexcept
    {
        return rootOverride >= 0 ? rootOverride : sample.rootNote;
    }

    /** For the waveform display. */
    std::atomic<int> lastNote { -1 };
    std::atomic<float> lastPosition { -1.0f };

    /** The key whose sample the Sample Lock would keep. Follows the last note played while unlocked. */
    std::atomic<int> selectedNote { SampleData::defaultRootNote };
    std::atomic<int> rootOverride { -1 };

private:
    enum class Stage { attack, decay, sustain, release };

    struct Grain
    {
        double position = 0.0;
        int age = 0, length = 0;
    };

    struct Voice
    {
        const SampleData* sample = nullptr;
        bool active = false, killing = false, keyDown = false, heldByPedal = false;
        bool oneShot = false, loops = false;
        int note = 0, direction = 1;
        double position = 0.0, startPos = 0.0, beginPos = 0.0, endPos = 0.0, baseIncrement = 1.0;
        Stage stage = Stage::attack;
        float envelope = 0.0f, killGain = 1.0f, wrapGain = 1.0f, velocityGain = 1.0f;
        float panLeft = 1.0f, panRight = 1.0f, chaosSemitones = 0.0f, keySemitones = 0.0f;
        juce::uint64 age = 0;
        // pitch shifting that keeps the length: two overlapping grains following the voice's position
        bool keepsLength = false, grainsRunning = false;
        Grain grains[2];
        int nextGrain = 0, grainCountdown = 0;
        // state-variable filter
        float ic1[2] { 0.0f, 0.0f }, ic2[2] { 0.0f, 0.0f };
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f, smoothedCutoff = 20000.0f;
        int filterCountdown = 0;
    };

    void updateCoefficients();
    void handleMidi (const juce::uint8* data, int numBytes);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void setPedal (bool isDown);
    void beginRelease (Voice& voice);
    void fadeAllVoices() noexcept;
    void renderVoices (juce::AudioBuffer<float>& buffer, int start, int count);
    void renderVoice (Voice& voice, float* left, float* right, int count);
    void updateFilter (Voice& voice);
    Voice* takeVoice();

    int grainLengthFor (double pitchRatio) const noexcept;
    void startGrains (Voice& voice, double pitchRatio) noexcept;
    void spawnGrain (Voice& voice, double pitchRatio) noexcept;
    double alignedGrainStart (const Voice& voice, const Grain& other, double nominalStart, double signedRate, int length) const noexcept;

    std::array<Voice, voicePool> voices;
    std::array<float, 1025> grainWindow;
    BankPtr bank;
    Params params;
    juce::Random random { 0x52796730 };
    double sampleRate = 44100.0;
    double pitchBendSt = 0.0;
    juce::uint64 ageCounter = 0;
    bool pedalDown = false;
    float attackStep = 1.0f, decayCoef = 0.0f, releaseCoef = 0.0f, killStep = 0.01f, wrapStep = 0.01f, edgeFadeFrames = 100.0f;
    bool filterActive = false;
};
}
