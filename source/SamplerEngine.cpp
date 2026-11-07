#include "SamplerEngine.h"
#include <cmath>

namespace rygo
{
namespace
{
constexpr float envelopeFloor = 0.0001f;     // -80 dB: the voice is finished
constexpr float minusSixtyDbLog = 6.9078f;   // ln(1000): times are measured to -60 dB
constexpr int filterUpdateInterval = 16;
constexpr int alignmentPoints = 48;

inline float hermite (const float* data, int index, float t, int numFrames) noexcept
{
    const auto ym1 = data[juce::jmax (index - 1, 0)];
    const auto y0  = data[index];
    const auto y1  = data[juce::jmin (index + 1, numFrames - 1)];
    const auto y2  = data[juce::jmin (index + 2, numFrames - 1)];

    const auto c1 = 0.5f * (y1 - ym1);
    const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

/** How far a voice can still go. A reversed voice also has its last frame, at beginPos, still to play. */
template <typename VoiceType>
inline double remainingFrames (const VoiceType& voice) noexcept
{
    return voice.direction > 0 ? voice.endPos - voice.position : voice.position - voice.beginPos + 1.0;
}

inline float readNearest (const float* data, double position, int numFrames) noexcept
{
    const auto index = (int) position;
    return juce::isPositiveAndBelow (index, numFrames) && position >= 0.0 ? data[index] : 0.0f;
}
}

SamplerEngine::SamplerEngine()
{
    // Hann: two of these half a window apart always add up to exactly 1.
    for (size_t i = 0; i < grainWindow.size(); ++i)
    {
        const auto s = std::sin (juce::MathConstants<float>::pi * (float) i / (float) (grainWindow.size() - 1));
        grainWindow[i] = s * s;
    }
}

void SamplerEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    killStep = (float) (1.0 / (0.005 * sampleRate));
    wrapStep = (float) (1.0 / (0.003 * sampleRate));
    edgeFadeFrames = (float) (0.003 * sampleRate);
    reset();
}

void SamplerEngine::reset()
{
    for (auto& voice : voices)
        voice.active = false;
    pedalDown = false;
    pitchBendSt = 0.0;
    lastNote = -1;
    lastPosition = -1.0f;
}

void SamplerEngine::setBank (BankPtr newBank) noexcept
{
    jassert (bank == nullptr);   // so nothing gets freed here, on the audio thread
    bank = std::move (newBank);
}

void SamplerEngine::fadeAllVoices() noexcept
{
    for (auto& voice : voices)
        if (voice.active)
            voice.killing = true;
}

void SamplerEngine::updateCoefficients()
{
    const auto rate = (float) sampleRate;
    attackStep = 1.0f / (juce::jmax (params.attackSec, 0.0005f) * rate);
    decayCoef = std::exp (-minusSixtyDbLog / (juce::jmax (params.decaySec, 0.001f) * rate));
    releaseCoef = std::exp (-minusSixtyDbLog / (juce::jmax (params.releaseSec, 0.001f) * rate));

    // A fully open filter is skipped, so the plain sampler path stays bit-transparent.
    const auto isOpen = (params.filterType == 0 && params.cutoffHz >= 19900.0f)
                     || (params.filterType == 1 && params.cutoffHz <= 21.0f);
    filterActive = ! (isOpen && params.resonance < 0.001f && std::abs (params.filterEnv) < 0.001f);
}

void SamplerEngine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Params& newParams)
{
    params = newParams;
    updateCoefficients();

    const auto numSamples = buffer.getNumSamples();
    int rendered = 0;

    // Render up to each event, so a note starts on its exact sample regardless of the host's buffer size.
    for (const auto event : midi)
    {
        const auto position = juce::jlimit (0, numSamples, event.samplePosition);
        if (position > rendered)
        {
            renderVoices (buffer, rendered, position - rendered);
            rendered = position;
        }
        handleMidi (event.data, event.numBytes);
    }

    if (rendered < numSamples)
        renderVoices (buffer, rendered, numSamples - rendered);

    // Publish the most recently started voice's position for the waveform view.
    const Voice* newest = nullptr;
    for (const auto& voice : voices)
        if (voice.active && (newest == nullptr || voice.age > newest->age))
            newest = &voice;

    lastPosition = newest != nullptr ? (float) (newest->position / juce::jmax (1, newest->sample->audio.getNumSamples()))
                                     : -1.0f;
}

void SamplerEngine::handleMidi (const juce::uint8* data, int numBytes)
{
    if (numBytes < 1)
        return;

    const auto type = data[0] & 0xf0;

    if (numBytes >= 3 && type == 0x90 && data[2] > 0)
        noteOn (data[1], (float) data[2] / 127.0f);
    else if (numBytes >= 3 && (type == 0x80 || type == 0x90))
        noteOff (data[1]);
    else if (numBytes >= 3 && type == 0xb0)
    {
        if (data[1] == 64)
            setPedal (data[2] >= 64);
        else if (data[1] == 120)
            fadeAllVoices();
        else if (data[1] == 123)
            for (auto& voice : voices)
                if (voice.active && ! voice.oneShot)
                    beginRelease (voice);
    }
    else if (numBytes >= 3 && type == 0xe0)
    {
        const auto bend = ((data[2] << 7) | data[1]) - 8192;
        pitchBendSt = 2.0 * bend / 8192.0;
    }
}

SamplerEngine::Voice* SamplerEngine::takeVoice()
{
    Voice* oldest = nullptr;
    Voice* oldestAudible = nullptr;
    Voice* free = nullptr;
    int audible = 0;

    for (auto& voice : voices)
    {
        if (! voice.active)
        {
            if (free == nullptr)
                free = &voice;
            continue;
        }

        if (oldest == nullptr || voice.age < oldest->age)
            oldest = &voice;

        if (! voice.killing)
        {
            ++audible;
            if (oldestAudible == nullptr || voice.age < oldestAudible->age)
                oldestAudible = &voice;
        }
    }

    if (audible >= polyLimit && oldestAudible != nullptr)
        oldestAudible->killing = true;

    return free != nullptr ? free : oldest;
}

void SamplerEngine::noteOn (int note, float velocity)
{
    lastNote = note;

    const auto sourceNote = params.locked ? selectedNote.load() : note;
    const auto* sample = bank != nullptr ? bank->sampleForNote (sourceNote) : nullptr;

    if (sample == nullptr)
        return;

    if (! params.locked && selectedNote.load() != note)
    {
        selectedNote = note;
        rootOverride = -1;   // a manual root belonged to the previous sample
    }

    // Repeating a key cuts its previous hit instead of stacking the same sound on top of itself.
    for (auto& voice : voices)
        if (voice.active && voice.note == note)
            voice.killing = true;

    auto* voice = takeVoice();
    if (voice == nullptr)
        return;

    *voice = Voice {};
    voice->active = true;
    voice->keyDown = true;
    voice->sample = sample;
    voice->note = note;
    voice->age = ++ageCounter;
    voice->baseIncrement = sample->sourceRate / sampleRate;
    voice->stage = Stage::attack;
    voice->oneShot = params.mode == PlayMode::oneShot;
    voice->loops = params.mode == PlayMode::loop;

    if (params.locked)
    {
        voice->keySemitones = (float) (note - effectiveRoot (*sample, rootOverride.load()));
        voice->keepsLength = params.keepLength;
    }

    const auto velocityCurve = velocity * velocity;
    voice->velocityGain = 1.0f - params.velocity * (1.0f - velocityCurve);

    auto reverse = params.reverse;

    if (params.chaosOn)
    {
        voice->chaosSemitones = (random.nextFloat() * 2.0f - 1.0f) * params.chaosPitch * 12.0f;
        if (random.nextFloat() < params.chaosReverse)
            reverse = ! reverse;

        const auto pan = (random.nextFloat() * 2.0f - 1.0f) * params.chaosPan;
        const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        voice->panLeft = std::cos (angle) * juce::MathConstants<float>::sqrt2;
        voice->panRight = std::sin (angle) * juce::MathConstants<float>::sqrt2;
    }

    const auto total = sample->audio.getNumSamples();
    voice->beginPos = params.trim ? sample->trimStart : 0;
    voice->endPos = params.trim ? sample->trimEnd : total;

    const auto offset = params.startFrac * (voice->endPos - voice->beginPos);
    voice->direction = reverse ? -1 : 1;
    voice->startPos = reverse ? voice->endPos - 1.0 - offset : voice->beginPos + offset;
    voice->position = voice->startPos;
    voice->smoothedCutoff = params.cutoffHz;
}

void SamplerEngine::beginRelease (Voice& voice)
{
    if (voice.stage != Stage::release)
        voice.stage = Stage::release;
}

void SamplerEngine::noteOff (int note)
{
    for (auto& voice : voices)
    {
        if (! voice.active || voice.note != note || ! voice.keyDown)
            continue;

        voice.keyDown = false;

        if (voice.oneShot)
            continue;

        if (pedalDown)
            voice.heldByPedal = true;
        else
            beginRelease (voice);
    }
}

void SamplerEngine::setPedal (bool isDown)
{
    pedalDown = isDown;
    if (isDown)
        return;

    for (auto& voice : voices)
    {
        if (voice.active && voice.heldByPedal)
        {
            voice.heldByPedal = false;
            beginRelease (voice);
        }
    }
}

void SamplerEngine::renderVoices (juce::AudioBuffer<float>& buffer, int start, int count)
{
    auto* left = buffer.getWritePointer (0, start);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : left;

    for (auto& voice : voices)
        if (voice.active)
            renderVoice (voice, left, right, count);
}

void SamplerEngine::updateFilter (Voice& voice)
{
    auto target = params.cutoffHz;
    if (std::abs (params.filterEnv) > 0.001f)
        target *= std::pow (2.0f, params.filterEnv * 6.0f * voice.envelope);

    voice.smoothedCutoff += 0.25f * (target - voice.smoothedCutoff);
    const auto cutoff = juce::jlimit (20.0f, (float) sampleRate * 0.45f, voice.smoothedCutoff);

    const auto g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
    const auto q = 0.707f + params.resonance * params.resonance * 12.0f;
    voice.k = 1.0f / q;
    voice.a1 = 1.0f / (1.0f + g * (g + voice.k));
    voice.a2 = g * voice.a1;
    voice.a3 = g * voice.a2;
}

int SamplerEngine::grainLengthFor (double pitchRatio) const noexcept
{
    // Long enough to hold a low note's period, short enough not to smear a pitched-up transient.
    const auto seconds = juce::jlimit (0.02, 0.08, 0.06 / std::sqrt (pitchRatio));
    return juce::jmax (8, juce::roundToInt (seconds * sampleRate * 0.5) * 2);
}

void SamplerEngine::startGrains (Voice& voice, double pitchRatio) noexcept
{
    // The first grain starts at the top of its window, so the sample's attack comes through intact.
    const auto length = grainLengthFor (pitchRatio);
    voice.grains[0] = { voice.position, length / 2, length };
    voice.grains[1] = {};
    voice.nextGrain = 1;
    voice.grainCountdown = 0;
    voice.grainsRunning = true;
}

double SamplerEngine::alignedGrainStart (const Voice& voice, const Grain& other, double nominalStart,
                                         double signedRate, int length) const noexcept
{
    // Grains laid over each other at random cancel out and warble. The new one starts where it
    // best matches what the fading grain is about to play.
    const auto* source = voice.sample->audio.getReadPointer (0);
    const auto numFrames = voice.sample->audio.getNumSamples();
    const auto overlap = juce::jmin (length / 2, other.length - other.age);
    const auto stride = juce::jmax (1, overlap / alignmentPoints);
    const auto points = juce::jmin (alignmentPoints, overlap / stride);

    if (points < 4)
        return nominalStart;

    float expected[alignmentPoints];
    for (int i = 0; i < points; ++i)
        expected[i] = readNearest (source, other.position + i * stride * signedRate, numFrames);

    const auto sourceRate = voice.sample->sourceRate;
    const auto reach = juce::roundToInt (sourceRate * 0.006);
    const auto step = juce::jmax (1, juce::roundToInt (sourceRate / 8000.0));

    auto bestScore = 0.0f;
    auto best = nominalStart;

    for (int lag = -reach; lag <= reach; lag += step)
    {
        const auto candidate = nominalStart + lag;
        auto product = 0.0f, energy = 0.000001f;

        for (int i = 0; i < points; ++i)
        {
            const auto value = readNearest (source, candidate + i * stride * signedRate, numFrames);
            product += expected[i] * value;
            energy += value * value;
        }

        const auto score = product / std::sqrt (energy);
        if (score > bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

void SamplerEngine::spawnGrain (Voice& voice, double pitchRatio) noexcept
{
    auto& next = voice.grains[voice.nextGrain];
    const auto& other = voice.grains[1 - voice.nextGrain];
    voice.nextGrain = 1 - voice.nextGrain;

    const auto length = grainLengthFor (pitchRatio);
    const auto signedRate = voice.baseIncrement * pitchRatio * voice.direction;

    // Its middle, where it's loudest, lines up with where the voice will be by then.
    const auto middle = length * 0.5;
    auto start = voice.position + middle * (voice.baseIncrement * voice.direction - signedRate);

    if (other.age < other.length)
        start = alignedGrainStart (voice, other, start, signedRate, length);

    next.position = juce::jlimit (voice.beginPos, juce::jmax (voice.beginPos, voice.endPos - 1.0), start);
    next.age = 0;
    next.length = length;
    voice.grainCountdown = length / 2;
}

void SamplerEngine::renderVoice (Voice& voice, float* left, float* right, int count)
{
    const auto& sample = *voice.sample;
    const auto* sourceLeft = sample.audio.getReadPointer (0);
    const auto* sourceRight = sample.audio.getNumChannels() > 1 ? sample.audio.getReadPointer (1) : sourceLeft;
    const auto numFrames = sample.audio.getNumSamples();

    const auto semitones = (double) params.pitchSt + pitchBendSt + (double) voice.chaosSemitones + (double) voice.keySemitones;
    const auto isShifted = ! juce::exactlyEqual (semitones, 0.0);
    const auto pitchRatio = isShifted ? std::pow (2.0, semitones / 12.0) : 1.0;
    const auto playRate = voice.baseIncrement * pitchRatio;
    const auto signedPlayRate = playRate * voice.direction;

    // With grains, the voice moves through the sample at its natural speed and only the grains are
    // read faster or slower: that is what keeps the length. At the root note nothing needs shifting.
    const auto useGrains = voice.keepsLength && isShifted;
    const auto increment = useGrains ? voice.baseIncrement : playRate;

    if (! useGrains)
        voice.grainsRunning = false;
    else if (! voice.grainsRunning)
        startGrains (voice, pitchRatio);

    const auto sampleGain = voice.velocityGain * (params.normalize ? sample.normGain : 1.0f);
    const auto sustain = params.sustain;
    const auto isDual = sourceRight == sourceLeft;

    for (int i = 0; i < count; ++i)
    {
        auto remaining = remainingFrames (voice);

        if (remaining <= 0.0)
        {
            const auto isHeld = voice.keyDown || voice.heldByPedal;
            if (! voice.loops || ! isHeld || voice.stage == Stage::release)
            {
                voice.active = false;
                return;
            }

            voice.position = voice.startPos;
            voice.wrapGain = 0.0f;
            remaining = remainingFrames (voice);

            if (useGrains)
                startGrains (voice, pitchRatio);
        }

        switch (voice.stage)
        {
            case Stage::attack:
                voice.envelope += attackStep;
                if (voice.envelope >= 1.0f)
                {
                    voice.envelope = 1.0f;
                    voice.stage = Stage::decay;
                }
                break;

            case Stage::decay:
                voice.envelope = sustain + (voice.envelope - sustain) * decayCoef;
                if (voice.envelope - sustain < envelopeFloor)
                {
                    voice.envelope = sustain;
                    voice.stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                voice.envelope += 0.001f * (sustain - voice.envelope);
                if (voice.envelope < envelopeFloor)
                {
                    voice.active = false;
                    return;
                }
                break;

            case Stage::release:
                voice.envelope *= releaseCoef;
                if (voice.envelope < envelopeFloor)
                {
                    voice.active = false;
                    return;
                }
                break;
        }

        auto gain = voice.envelope * sampleGain;

        // The file may end abruptly, so the last few milliseconds always fade.
        const auto framesLeft = (float) (remaining / increment);
        if (framesLeft < edgeFadeFrames)
            gain *= framesLeft / edgeFadeFrames;

        // And a loop's restart fades back in.
        if (voice.wrapGain < 1.0f)
        {
            voice.wrapGain = juce::jmin (1.0f, voice.wrapGain + wrapStep);
            gain *= voice.wrapGain;
        }

        if (voice.killing)
        {
            voice.killGain -= killStep;
            if (voice.killGain <= 0.0f)
            {
                voice.active = false;
                return;
            }
            gain *= voice.killGain;
        }

        auto outLeft = 0.0f, outRight = 0.0f;

        if (useGrains)
        {
            if (--voice.grainCountdown <= 0)
                spawnGrain (voice, pitchRatio);

            for (auto& grain : voice.grains)
            {
                if (grain.age >= grain.length)
                    continue;

                const auto window = grainWindow[(size_t) (grain.age * (int) (grainWindow.size() - 1) / grain.length)];

                if (grain.position >= 0.0 && grain.position < (double) numFrames)
                {
                    const auto index = (int) grain.position;
                    const auto fraction = (float) (grain.position - index);
                    const auto valueLeft = hermite (sourceLeft, index, fraction, numFrames);
                    outLeft += valueLeft * window;
                    outRight += (isDual ? valueLeft : hermite (sourceRight, index, fraction, numFrames)) * window;
                }

                grain.position += signedPlayRate;
                ++grain.age;
            }
        }
        else
        {
            const auto index = juce::jmax (0, (int) voice.position);
            const auto fraction = juce::jmax (0.0f, (float) (voice.position - index));
            outLeft = hermite (sourceLeft, index, fraction, numFrames);
            outRight = isDual ? outLeft : hermite (sourceRight, index, fraction, numFrames);
        }

        if (filterActive)
        {
            if (--voice.filterCountdown <= 0)
            {
                updateFilter (voice);
                voice.filterCountdown = filterUpdateInterval;
            }

            float* samples[2] { &outLeft, &outRight };
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto input = *samples[channel];
                const auto v3 = input - voice.ic2[channel];
                const auto v1 = voice.a1 * voice.ic1[channel] + voice.a2 * v3;
                const auto v2 = voice.ic2[channel] + voice.a2 * voice.ic1[channel] + voice.a3 * v3;
                voice.ic1[channel] = 2.0f * v1 - voice.ic1[channel];
                voice.ic2[channel] = 2.0f * v2 - voice.ic2[channel];

                *samples[channel] = params.filterType == 0 ? v2
                                  : params.filterType == 1 ? input - voice.k * v1 - v2
                                                           : v1;
            }
        }

        left[i] += outLeft * gain * voice.panLeft;
        right[i] += outRight * gain * voice.panRight;

        voice.position += increment * voice.direction;
    }
}
}
