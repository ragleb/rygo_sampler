#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace rygo
{
namespace
{
constexpr const char* programProperty = "program";
constexpr const char* selectedNoteProperty = "selectedNote";
constexpr const char* rootOverrideProperty = "rootOverride";

// A parameter's default of 0 comes back from its normalised form with rounding error. Unnoticed, that
// detunes every sample by a hair and lets it drift out of phase with the source: exactly zero means zero.
float snapToZero (float value) noexcept
{
    return std::abs (value) < 0.0005f ? 0.0f : value;
}
}

RygoProcessor::RygoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "RygostancijaState", createParameterLayout())
{
    const auto get = [this] (const char* id)
    {
        auto* value = state.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    raw = { get (pid::attack), get (pid::decay), get (pid::sustain), get (pid::release), get (pid::velocity),
            get (pid::filterType), get (pid::cutoff), get (pid::resonance), get (pid::filterEnv),
            get (pid::pitch), get (pid::start), get (pid::gain), get (pid::mode), get (pid::reverse),
            get (pid::trimSilence), get (pid::normalize),
            get (pid::musicOn), get (pid::musicLock), get (pid::musicKeepLength),
            get (pid::fxOn), get (pid::chaosOn), get (pid::chaosPitch), get (pid::chaosReverse), get (pid::chaosPan),
            get (pid::stutterOn), get (pid::stutterRate), get (pid::stutterChance),
            get (pid::dirtOn), get (pid::dirtDrive), get (pid::dirtBits), get (pid::dirtRate),
            get (pid::echoOn), get (pid::echoTime), get (pid::echoFeedback), get (pid::echoMix),
            get (pid::reverbOn), get (pid::reverbMix), get (pid::reverbSize), get (pid::reverbDamp) };

    loadFinished.reset();

    loader.addJob ([this]
    {
        auto bank = library->loadBank ([this] { return shuttingDown.load(); });
        if (bank == nullptr)
            return;

        // The selected key must hold a sample, or the lock would silence the keyboard.
        if (bank->sampleForNote (engine.selectedNote.load()) == nullptr && ! bank->samples.empty())
            engine.selectedNote = bank->firstNote;

        {
            const juce::ScopedLock scoped (loadedBankLock);
            loadedBank = bank;
        }
        {
            const juce::SpinLock::ScopedLockType scoped (handOverLock);
            bankForAudioThread = std::move (bank);
            bankReady = true;
        }

        loading = false;
        loadFinished.signal();
    });
}

RygoProcessor::~RygoProcessor()
{
    shuttingDown = true;
    loader.removeAllJobs (true, 10000);
}

bool RygoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RygoProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, readFxParams());
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (snapToZero (raw.gain->load()), -60.0f));
    keyboardState.reset();
    adoptLoadedBank();
}

SamplerEngine::Params RygoProcessor::readEngineParams() const noexcept
{
    SamplerEngine::Params p;
    p.attackSec = raw.attack->load() * 0.001f;
    p.decaySec = raw.decay->load() * 0.001f;
    p.sustain = raw.sustain->load() * 0.01f;
    p.releaseSec = raw.release->load() * 0.001f;
    p.velocity = raw.velocity->load() * 0.01f;
    p.filterType = (int) raw.filterType->load();
    p.cutoffHz = raw.cutoff->load();
    p.resonance = raw.resonance->load() * 0.01f;
    p.filterEnv = raw.filterEnv->load() * 0.01f;
    p.pitchSt = snapToZero (raw.pitch->load());
    p.startFrac = raw.start->load() * 0.01f;
    p.mode = (SamplerEngine::PlayMode) juce::jlimit (0, 2, (int) raw.mode->load());
    p.reverse = raw.reverse->load() > 0.5f;
    p.trim = raw.trimSilence->load() > 0.5f;
    p.normalize = raw.normalize->load() > 0.5f;
    p.chaosOn = raw.fxOn->load() > 0.5f && raw.chaosOn->load() > 0.5f;
    p.chaosPitch = raw.chaosPitch->load() * 0.01f;
    p.chaosReverse = raw.chaosReverse->load() * 0.01f;
    p.chaosPan = raw.chaosPan->load() * 0.01f;
    p.locked = isSampleLocked();
    p.keepLength = raw.musicKeepLength->load() > 0.5f;
    return p;
}

FxChain::Params RygoProcessor::readFxParams() const noexcept
{
    FxChain::Params p;
    p.masterOn = raw.fxOn->load() > 0.5f;
    p.dirtOn = raw.dirtOn->load() > 0.5f;
    p.stutterOn = raw.stutterOn->load() > 0.5f;
    p.echoOn = raw.echoOn->load() > 0.5f;
    p.reverbOn = raw.reverbOn->load() > 0.5f;
    p.dirtDriveDb = raw.dirtDrive->load();
    p.dirtBits = raw.dirtBits->load();
    p.dirtDownsample = raw.dirtRate->load();
    p.stutterBeats = stutterBeats[(size_t) juce::jlimit (0, (int) stutterBeats.size() - 1, (int) raw.stutterRate->load())];
    p.stutterChance = raw.stutterChance->load() * 0.01f;
    p.echoBeats = echoBeats[(size_t) juce::jlimit (0, (int) echoBeats.size() - 1, (int) raw.echoTime->load())];
    p.echoFeedback = raw.echoFeedback->load() * 0.01f;
    p.echoMix = raw.echoMix->load() * 0.01f;
    p.reverbMix = raw.reverbMix->load() * 0.01f;
    p.reverbSize = raw.reverbSize->load() * 0.01f;
    p.reverbDamp = raw.reverbDamp->load() * 0.01f;
    return p;
}

FxChain::Transport RygoProcessor::readTransport() const
{
    FxChain::Transport transport;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            transport.bpm = position->getBpm().orFallback (120.0);
            transport.isPlaying = position->getIsPlaying();
            transport.ppq = position->getPpqPosition().orFallback (0.0);
        }
    }
    return transport;
}

void RygoProcessor::adoptLoadedBank() noexcept
{
    if (! bankReady.load())
        return;

    const juce::SpinLock::ScopedTryLockType scoped (handOverLock);
    if (! scoped.isLocked())
        return;

    engine.setBank (std::move (bankForAudioThread));
    bankForAudioThread = nullptr;
    bankReady = false;
}

void RygoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // During an offline render nobody is waiting on the audio thread, and a note must not be
    // skipped just because the samples are still loading. In real time, it plays silence meanwhile.
    if (isNonRealtime() && loading.load())
        loadFinished.wait (30000);

    adoptLoadedBank();

    const auto numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    engine.process (buffer, midi, readEngineParams());
    fx.process (buffer, readFxParams(), readTransport());

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (snapToZero (raw.gain->load()), -60.0f));
    masterGain.applyGain (buffer, numSamples);

    midi.clear();
}

BankPtr RygoProcessor::getLoadedBank() const
{
    const juce::ScopedLock scoped (loadedBankLock);
    return loadedBank;
}

bool RygoProcessor::isSampleLocked() const noexcept
{
    return raw.musicOn->load() > 0.5f && raw.musicLock->load() > 0.5f;
}

void RygoProcessor::selectNote (int note)
{
    if (isSampleLocked() || note == engine.selectedNote.load())
        return;

    engine.selectedNote = note;
    engine.rootOverride = -1;
}

void RygoProcessor::setRootOverride (int note)
{
    engine.rootOverride = note;
}

juce::AudioProcessorEditor* RygoProcessor::createEditor()
{
    return new RygoEditor (*this);
}

int RygoProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

void RygoProcessor::setCurrentProgram (int index)
{
    // Some hosts call this with the current index right after restoring a session;
    // re-applying the preset then would wipe the restored settings.
    if (index != currentProgram)
        applyPreset (index);
}

const juce::String RygoProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void RygoProcessor::applyPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    const auto& preset = presets[(size_t) index];

    for (auto* item : getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (item);
        if (parameter == nullptr)
            continue;

        // A preset is a sound. It leaves the Music section alone, so browsing presets
        // doesn't unlock a sample the user is playing.
        const auto id = parameter->getParameterID();
        if (id == pid::musicOn || id == pid::musicLock || id == pid::musicKeepLength)
            continue;

        auto normalised = parameter->getDefaultValue();
        for (const auto& [presetId, value] : preset.values)
            if (id == presetId)
                normalised = parameter->convertTo0to1 (value);

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    }
}

void RygoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty (programProperty, currentProgram, nullptr);
    tree.setProperty (selectedNoteProperty, engine.selectedNote.load(), nullptr);
    tree.setProperty (rootOverrideProperty, engine.rootOverride.load(), nullptr);

    if (const auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void RygoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    state.replaceState (tree);
    currentProgram = tree.getProperty (programProperty, 0);
    engine.selectedNote = juce::jlimit (0, 127, (int) tree.getProperty (selectedNoteProperty, SampleData::defaultRootNote));
    engine.rootOverride = juce::jlimit (-1, 127, (int) tree.getProperty (rootOverrideProperty, -1));
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new rygo::RygoProcessor();
}
