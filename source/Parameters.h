#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <utility>
#include <vector>

namespace rygo
{
namespace pid
{
#define RYGO_PID(name) inline constexpr const char* name = #name;
    // Envelope
    RYGO_PID (attack)
    RYGO_PID (decay)
    RYGO_PID (sustain)
    RYGO_PID (release)
    RYGO_PID (velocity)
    // Filter
    RYGO_PID (filterType)
    RYGO_PID (cutoff)
    RYGO_PID (resonance)
    RYGO_PID (filterEnv)
    // Sample playback
    RYGO_PID (pitch)
    RYGO_PID (start)
    RYGO_PID (gain)
    RYGO_PID (mode)
    RYGO_PID (reverse)
    RYGO_PID (trimSilence)
    RYGO_PID (normalize)
    // Music mode: one sample pitched across the keyboard
    RYGO_PID (musicOn)
    RYGO_PID (musicLock)
    RYGO_PID (musicKeepLength)
    // Effects
    RYGO_PID (fxOn)
    RYGO_PID (chaosOn)
    RYGO_PID (chaosPitch)
    RYGO_PID (chaosReverse)
    RYGO_PID (chaosPan)
    RYGO_PID (stutterOn)
    RYGO_PID (stutterRate)
    RYGO_PID (stutterChance)
    RYGO_PID (dirtOn)
    RYGO_PID (dirtDrive)
    RYGO_PID (dirtBits)
    RYGO_PID (dirtRate)
    RYGO_PID (echoOn)
    RYGO_PID (echoTime)
    RYGO_PID (echoFeedback)
    RYGO_PID (echoMix)
    RYGO_PID (reverbOn)
    RYGO_PID (reverbMix)
    RYGO_PID (reverbSize)
    RYGO_PID (reverbDamp)
#undef RYGO_PID
}

/** Note lengths in quarter notes, in the order of the matching choice parameters. */
inline constexpr std::array<double, 7> stutterBeats { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.0625 };
inline constexpr std::array<double, 9> echoBeats { 0.125, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;   // real units; anything not listed gets its default
};

const std::vector<FactoryPreset>& getFactoryPresets();

/** Number parsing shared with the UI: accepts "1,5", "−3" and Russian or English units. */
float parseTypedNumber (const juce::String& text);
}
