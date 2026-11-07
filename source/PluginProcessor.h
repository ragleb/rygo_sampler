#pragma once

#include "FxChain.h"
#include "Parameters.h"
#include "SampleLibrary.h"
#include "SamplerEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

namespace rygo
{
class RygoProcessor final : public juce::AudioProcessor
{
public:
    RygoProcessor();
    ~RygoProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rygostancija"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Applies a factory preset even if it's already the current one, e.g. to discard edits. */
    void applyPreset (int index);

    BankPtr getLoadedBank() const;
    bool isBankLoading() const noexcept { return loading.load(); }

    /** Music mode. The selected sample is the key it lives on; the lock keeps it. */
    bool isSampleLocked() const noexcept;
    int getSelectedNote() const noexcept { return engine.selectedNote.load(); }
    void selectNote (int note);
    int getRootOverride() const noexcept { return engine.rootOverride.load(); }
    void setRootOverride (int note);

    SampleLibrary& getLibrary() { return *library; }
    juce::AudioProcessorValueTreeState& getState() { return state; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    SamplerEngine& getEngine() { return engine; }

private:
    struct RawParams
    {
        std::atomic<float>* attack, *decay, *sustain, *release, *velocity;
        std::atomic<float>* filterType, *cutoff, *resonance, *filterEnv;
        std::atomic<float>* pitch, *start, *gain, *mode, *reverse, *trimSilence, *normalize;
        std::atomic<float>* musicOn, *musicLock, *musicKeepLength;
        std::atomic<float>* fxOn, *chaosOn, *chaosPitch, *chaosReverse, *chaosPan;
        std::atomic<float>* stutterOn, *stutterRate, *stutterChance;
        std::atomic<float>* dirtOn, *dirtDrive, *dirtBits, *dirtRate;
        std::atomic<float>* echoOn, *echoTime, *echoFeedback, *echoMix;
        std::atomic<float>* reverbOn, *reverbMix, *reverbSize, *reverbDamp;
    };

    void adoptLoadedBank() noexcept;
    SamplerEngine::Params readEngineParams() const noexcept;
    FxChain::Params readFxParams() const noexcept;
    FxChain::Transport readTransport() const;

    juce::SharedResourcePointer<SampleLibrary> library;
    juce::AudioProcessorValueTreeState state;
    RawParams raw;
    juce::MidiKeyboardState keyboardState;
    SamplerEngine engine;
    FxChain fx;
    juce::SmoothedValue<float> masterGain;
    int currentProgram = 0;

    // The samples load once, on a background thread, and are handed over to the audio thread here.
    juce::SpinLock handOverLock;
    BankPtr bankForAudioThread;
    std::atomic<bool> bankReady { false };

    mutable juce::CriticalSection loadedBankLock;
    BankPtr loadedBank;

    std::atomic<bool> loading { true };
    std::atomic<bool> shuttingDown { false };
    juce::WaitableEvent loadFinished { true };
    juce::ThreadPool loader { juce::ThreadPoolOptions().withNumberOfThreads (1).withThreadName ("Rygostancija loader") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RygoProcessor)
};
}
