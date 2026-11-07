#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace rygo
{
/** Master effects: Dirt -> Stutter -> Echo -> Reverb. Allocates only in prepare(). */
class FxChain
{
public:
    struct Params
    {
        bool masterOn = true;
        bool dirtOn = false, stutterOn = false, echoOn = false, reverbOn = false;
        float dirtDriveDb = 12.0f, dirtBits = 10.0f, dirtDownsample = 2.0f;
        double stutterBeats = 0.25;
        float stutterChance = 0.5f;
        double echoBeats = 0.5;
        float echoFeedback = 0.4f, echoMix = 0.3f;
        float reverbMix = 0.25f, reverbSize = 0.6f, reverbDamp = 0.4f;
    };

    struct Transport
    {
        double bpm = 120.0;
        double ppq = 0.0;
        bool isPlaying = false;
    };

    /** Effects start in their current on/off state, so the first note after loading a project already sounds right. */
    void prepare (double sampleRate, const Params& initial);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Params& params, const Transport& transport);

private:
    static constexpr int chunkSize = 256;

    void processChunk (float* left, float* right, int count, const Params& params, double beatsPerSample);
    void dirt (float* left, float* right, int count, const Params& params);
    void stutter (float* left, float* right, int count, const Params& params, double beatsPerSample);
    void echo (float* left, float* right, int count, const Params& params, double secondsPerBeat);
    void applyReverb (float* left, float* right, int count, const Params& params);
    void resetStates();

    double sampleRate = 44100.0;
    double ppq = 0.0;
    double bpm = 120.0;

    juce::SmoothedValue<float> masterMix, dirtMix, stutterMix, echoAmount, reverbAmount;
    bool wasRunning = false;
    float dryLeft[chunkSize] {}, dryRight[chunkSize] {}, wetLeft[chunkSize] {}, wetRight[chunkSize] {};

    // Dirt
    float holdPhase = 1.0f, holdLeft = 0.0f, holdRight = 0.0f;

    // Stutter (beat repeat): loops the slice that has just played
    std::vector<float> ringLeft, ringRight;
    int ringWrite = 0, loopStart = 0, loopLength = 0, loopPhase = 0;
    juce::int64 lastSlice = -1;
    bool isRepeating = false;
    float repeatGain = 0.0f, repeatCoef = 0.01f;
    int repeatFade = 96;
    juce::Random random { 0x5a485249 };

    // Echo (ping-pong: the dual-mono samples get their width from here)
    std::vector<float> echoLeft, echoRight;
    int echoWrite = 0;
    float echoDelay = 0.0f, dampLeft = 0.0f, dampRight = 0.0f, dampCoef = 0.5f;

    // Reverb
    juce::Reverb reverb;
};
}
