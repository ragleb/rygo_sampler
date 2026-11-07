#include "FxChain.h"
#include <cmath>

namespace rygo
{
void FxChain::prepare (double newSampleRate, const Params& initial)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    const auto ringSize = (size_t) (sampleRate * 8.0) + 1;
    ringLeft.assign (ringSize, 0.0f);
    ringRight.assign (ringSize, 0.0f);

    const auto echoSize = (size_t) (sampleRate * 6.0) + 4;
    echoLeft.assign (echoSize, 0.0f);
    echoRight.assign (echoSize, 0.0f);

    repeatCoef = 1.0f - std::exp (-1.0f / (float) (0.002 * sampleRate));
    repeatFade = juce::jmax (1, juce::roundToInt (0.002 * sampleRate));
    dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 6000.0f / (float) sampleRate);

    reverb.setSampleRate (sampleRate);

    for (auto* mix : { &masterMix, &dirtMix, &stutterMix, &echoAmount, &reverbAmount })
        mix->reset (sampleRate, 0.02);

    reset();

    masterMix.setCurrentAndTargetValue (initial.masterOn ? 1.0f : 0.0f);
    dirtMix.setCurrentAndTargetValue (initial.dirtOn ? 1.0f : 0.0f);
    stutterMix.setCurrentAndTargetValue (initial.stutterOn ? 1.0f : 0.0f);
    echoAmount.setCurrentAndTargetValue (initial.echoOn ? 1.0f : 0.0f);
    reverbAmount.setCurrentAndTargetValue (initial.reverbOn ? 1.0f : 0.0f);
}

void FxChain::reset()
{
    for (auto* mix : { &dirtMix, &stutterMix, &echoAmount, &reverbAmount })
        mix->setCurrentAndTargetValue (0.0f);

    resetStates();
    ppq = 0.0;
    lastSlice = -1;
}

void FxChain::resetStates()
{
    std::fill (ringLeft.begin(), ringLeft.end(), 0.0f);
    std::fill (ringRight.begin(), ringRight.end(), 0.0f);
    std::fill (echoLeft.begin(), echoLeft.end(), 0.0f);
    std::fill (echoRight.begin(), echoRight.end(), 0.0f);
    ringWrite = loopStart = loopLength = loopPhase = echoWrite = 0;
    isRepeating = false;
    repeatGain = 0.0f;
    holdPhase = 1.0f;
    holdLeft = holdRight = 0.0f;
    dampLeft = dampRight = 0.0f;
    echoDelay = 0.0f;
    reverb.reset();
}

void FxChain::process (juce::AudioBuffer<float>& buffer, const Params& params, const Transport& transport)
{
    if (buffer.getNumChannels() < 2)
        return;

    bpm = juce::jlimit (20.0, 400.0, transport.bpm > 0.0 ? transport.bpm : 120.0);
    const auto beatsPerSample = bpm / 60.0 / sampleRate;

    if (transport.isPlaying)
        ppq = transport.ppq;

    masterMix.setTargetValue (params.masterOn ? 1.0f : 0.0f);

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto numSamples = buffer.getNumSamples();

    for (int done = 0; done < numSamples; done += chunkSize)
        processChunk (left + done, right + done, juce::jmin (chunkSize, numSamples - done), params, beatsPerSample);
}

void FxChain::processChunk (float* left, float* right, int count, const Params& params, double beatsPerSample)
{
    dirtMix.setTargetValue (params.dirtOn ? 1.0f : 0.0f);
    stutterMix.setTargetValue (params.stutterOn ? 1.0f : 0.0f);
    echoAmount.setTargetValue (params.echoOn ? 1.0f : 0.0f);
    reverbAmount.setTargetValue (params.reverbOn ? 1.0f : 0.0f);

    const auto isIdle = [] (const juce::SmoothedValue<float>& mix)
    {
        return ! mix.isSmoothing() && mix.getCurrentValue() <= 0.0f;
    };

    const auto allModulesOff = isIdle (dirtMix) && isIdle (stutterMix) && isIdle (echoAmount) && isIdle (reverbAmount);

    if (isIdle (masterMix) || allModulesOff)
    {
        // Nothing to do: the plain sampler signal passes through untouched.
        if (wasRunning)
        {
            resetStates();
            wasRunning = false;
        }
        ppq += count * beatsPerSample;
        return;
    }

    wasRunning = true;

    const auto crossfadeMaster = masterMix.isSmoothing() || masterMix.getCurrentValue() < 1.0f;
    if (crossfadeMaster)
    {
        juce::FloatVectorOperations::copy (dryLeft, left, count);
        juce::FloatVectorOperations::copy (dryRight, right, count);
    }

    if (! isIdle (dirtMix))        dirt (left, right, count, params);
    if (! isIdle (stutterMix))     stutter (left, right, count, params, beatsPerSample);
    if (! isIdle (echoAmount))     echo (left, right, count, params, 60.0 / bpm);
    if (! isIdle (reverbAmount))   applyReverb (left, right, count, params);

    if (crossfadeMaster)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto mix = masterMix.getNextValue();
            left[i] = dryLeft[i] + mix * (left[i] - dryLeft[i]);
            right[i] = dryRight[i] + mix * (right[i] - dryRight[i]);
        }
    }

    ppq += count * beatsPerSample;
}

void FxChain::dirt (float* left, float* right, int count, const Params& params)
{
    const auto drive = juce::Decibels::decibelsToGain (params.dirtDriveDb);
    const auto makeup = 1.0f / std::sqrt (drive);   // keeps perceived level roughly constant as drive rises
    const auto step = params.dirtBits >= 23.9f ? 0.0f : 2.0f / std::pow (2.0f, params.dirtBits);
    const auto holdIncrement = 1.0f / juce::jmax (1.0f, params.dirtDownsample);

    const auto crush = [=] (float input)
    {
        const auto shaped = std::tanh (input * drive) * makeup;
        return step > 0.0f ? std::round (shaped / step) * step : shaped;
    };

    for (int i = 0; i < count; ++i)
    {
        holdPhase += holdIncrement;
        if (holdPhase >= 1.0f)
        {
            holdPhase -= 1.0f;
            holdLeft = crush (left[i]);
            holdRight = crush (right[i]);
        }

        const auto mix = dirtMix.getNextValue();
        left[i] += mix * (holdLeft - left[i]);
        right[i] += mix * (holdRight - right[i]);
    }
}

void FxChain::stutter (float* left, float* right, int count, const Params& params, double beatsPerSample)
{
    const auto ringSize = (int) ringLeft.size();
    if (ringSize < 8)
        return;

    const auto sliceLength = juce::jlimit (8, ringSize - 1, juce::roundToInt (params.stutterBeats / beatsPerSample));

    for (int i = 0; i < count; ++i)
    {
        const auto beat = ppq + i * beatsPerSample;
        const auto slice = (juce::int64) std::floor (beat / params.stutterBeats);

        if (slice != lastSlice)
        {
            lastSlice = slice;

            if (random.nextFloat() < params.stutterChance)
            {
                if (! isRepeating)
                {
                    isRepeating = true;
                    loopLength = sliceLength;
                    loopStart = (ringWrite - sliceLength + ringSize) % ringSize;
                    loopPhase = 0;
                }
            }
            else
            {
                isRepeating = false;
            }
        }

        ringLeft[(size_t) ringWrite] = left[i];
        ringRight[(size_t) ringWrite] = right[i];
        ringWrite = (ringWrite + 1) % ringSize;

        auto loopedLeft = 0.0f;
        auto loopedRight = 0.0f;

        if (loopLength > 0)
        {
            const auto read = (size_t) ((loopStart + loopPhase) % ringSize);
            const auto edge = juce::jmin (loopPhase, loopLength - 1 - loopPhase);
            const auto window = juce::jmin (1.0f, (float) edge / (float) repeatFade);
            loopedLeft = ringLeft[read] * window;
            loopedRight = ringRight[read] * window;
            loopPhase = (loopPhase + 1) % loopLength;
        }

        repeatGain += repeatCoef * ((isRepeating ? 1.0f : 0.0f) - repeatGain);
        const auto mix = stutterMix.getNextValue() * repeatGain;
        left[i] += mix * (loopedLeft - left[i]);
        right[i] += mix * (loopedRight - right[i]);
    }
}

void FxChain::echo (float* left, float* right, int count, const Params& params, double secondsPerBeat)
{
    const auto size = (int) echoLeft.size();
    if (size < 4)
        return;

    const auto target = (float) juce::jlimit (1.0, (double) size - 3.0, params.echoBeats * secondsPerBeat * sampleRate);
    if (echoDelay <= 0.0f)
        echoDelay = target;

    const auto read = [size] (const std::vector<float>& line, float position)
    {
        const auto wrapped = position < 0.0f ? position + (float) size : position;
        const auto index = (int) wrapped;
        const auto fraction = wrapped - (float) index;
        const auto a = line[(size_t) (index % size)];
        const auto b = line[(size_t) ((index + 1) % size)];
        return a + fraction * (b - a);
    };

    for (int i = 0; i < count; ++i)
    {
        echoDelay += 0.0005f * (target - echoDelay);   // gliding avoids clicks when the time or tempo changes

        const auto position = (float) echoWrite - echoDelay;
        const auto delayedLeft = read (echoLeft, position);
        const auto delayedRight = read (echoRight, position);

        dampLeft += dampCoef * (delayedLeft - dampLeft);
        dampRight += dampCoef * (delayedRight - dampRight);

        const auto input = 0.5f * (left[i] + right[i]);
        echoLeft[(size_t) echoWrite] = input + params.echoFeedback * dampRight;
        echoRight[(size_t) echoWrite] = params.echoFeedback * dampLeft;
        echoWrite = (echoWrite + 1) % size;

        const auto mix = echoAmount.getNextValue() * params.echoMix;
        left[i] += mix * (delayedLeft - left[i]);
        right[i] += mix * (delayedRight - right[i]);
    }
}

void FxChain::applyReverb (float* left, float* right, int count, const Params& params)
{
    juce::Reverb::Parameters settings;
    settings.roomSize = params.reverbSize;
    settings.damping = params.reverbDamp;
    settings.wetLevel = 1.0f / 3.0f;   // juce::Reverb scales wet by 3, so this is unity
    settings.dryLevel = 0.0f;
    settings.width = 1.0f;
    settings.freezeMode = 0.0f;
    reverb.setParameters (settings);

    juce::FloatVectorOperations::copy (wetLeft, left, count);
    juce::FloatVectorOperations::copy (wetRight, right, count);
    reverb.processStereo (wetLeft, wetRight, count);

    for (int i = 0; i < count; ++i)
    {
        const auto mix = reverbAmount.getNextValue() * params.reverbMix;
        left[i] += mix * (wetLeft[i] - left[i]);
        right[i] += mix * (wetRight[i] - right[i]);
    }
}
}
