#include "Parameters.h"
#include "SampleLibrary.h"

namespace rygo
{
float parseTypedNumber (const juce::String& text)
{
    return text.replaceCharacter (',', '.')
               .replace (ru ("−"), "-")
               .retainCharacters ("+-0123456789.")
               .getFloatValue();
}

namespace
{
using juce::String;
using Float = juce::AudioParameterFloat;
using Attributes = juce::AudioParameterFloatAttributes;
using Choice = juce::AudioParameterChoice;
using Bool = juce::AudioParameterBool;

juce::ParameterID id (const char* name) { return { name, 1 }; }

juce::NormalisableRange<float> skewed (float low, float high, float centre)
{
    juce::NormalisableRange<float> range (low, high);
    range.setSkewForCentre (centre);
    return range;
}

Attributes msAttributes()
{
    return Attributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            if (value < 10.0f)     return String (value, 1) + " ms";
            if (value < 1000.0f)   return String (juce::roundToInt (value)) + " ms";
            return String (value / 1000.0f, 2) + " s";
        })
        .withValueFromStringFunction ([] (const String& text)
        {
            const auto lower = text.toLowerCase();
            const auto number = parseTypedNumber (text);
            const auto isMs = lower.contains ("ms") || lower.contains (ru ("мс"));
            const auto isSeconds = ! isMs && (lower.contains ("s") || lower.contains (ru ("с")));
            return isSeconds ? number * 1000.0f : number;
        });
}

Attributes unitAttributes (const String& unit, int decimals)
{
    return Attributes()
        .withStringFromValueFunction ([unit, decimals] (float value, int)
        {
            // Rounding error around zero must not show up as "-0.00".
            const auto smallest = 0.5f * std::pow (10.0f, (float) -decimals);
            const auto shown = std::abs (value) < smallest ? 0.0f : value;
            return String (shown, decimals) + (unit.isEmpty() ? String() : " " + unit);
        })
        .withValueFromStringFunction ([] (const String& text) { return parseTypedNumber (text); });
}

Attributes percentAttributes() { return unitAttributes ("%", 0); }

Attributes hertzAttributes()
{
    return Attributes()
        .withStringFromValueFunction ([] (float value, int)
        {
            return value < 1000.0f ? String (juce::roundToInt (value)) + " Hz"
                                   : String (value / 1000.0f, 2) + " kHz";
        })
        .withValueFromStringFunction ([] (const String& text)
        {
            const auto lower = text.toLowerCase();
            const auto number = parseTypedNumber (text);
            return lower.contains ("k") || lower.contains (ru ("к")) ? number * 1000.0f : number;
        });
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto addFloat = [&layout] (const char* paramId, const char* name, juce::NormalisableRange<float> range,
                                     float defaultValue, Attributes attributes)
    {
        layout.add (std::make_unique<Float> (id (paramId), name, range, defaultValue, attributes));
    };
    const auto addBool = [&layout] (const char* paramId, const char* name, bool defaultValue)
    {
        layout.add (std::make_unique<Bool> (id (paramId), name, defaultValue));
    };
    const auto addChoice = [&layout] (const char* paramId, const char* name, juce::StringArray choices, int defaultIndex)
    {
        layout.add (std::make_unique<Choice> (id (paramId), name, choices, defaultIndex));
    };
    const juce::NormalisableRange<float> percent (0.0f, 100.0f);

    addFloat (pid::attack,   "Attack",   skewed (0.0f, 5000.0f, 150.0f),  1.0f,   msAttributes());
    addFloat (pid::decay,    "Decay",    skewed (1.0f, 10000.0f, 500.0f), 500.0f, msAttributes());
    addFloat (pid::sustain,  "Sustain",  percent,                         100.0f, percentAttributes());
    addFloat (pid::release,  "Release",  skewed (1.0f, 10000.0f, 400.0f), 60.0f,  msAttributes());
    addFloat (pid::velocity, "Velocity", percent,                         100.0f, percentAttributes());

    addChoice (pid::filterType, "Filter Type", { "LP", "HP", "BP" }, 0);
    addFloat (pid::cutoff,    "Cutoff",     skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hertzAttributes());
    addFloat (pid::resonance, "Resonance",  percent,                           0.0f,     percentAttributes());
    addFloat (pid::filterEnv, "Filter Env", { -100.0f, 100.0f },               0.0f,     percentAttributes());

    addFloat (pid::pitch, "Pitch", { -24.0f, 24.0f },        0.0f, unitAttributes ("st", 2));
    addFloat (pid::start, "Start", { 0.0f, 90.0f },          0.0f, percentAttributes());
    addFloat (pid::gain,  "Gain",  { -60.0f, 12.0f },        0.0f, unitAttributes ("dB", 1));
    addChoice (pid::mode, "Mode", { "One Shot", "Gate", "Loop" }, 1);
    addBool (pid::reverse,     "Reverse",      false);
    addBool (pid::trimSilence, "Trim Silence", true);
    addBool (pid::normalize,   "Normalize",    true);

    addBool (pid::musicOn, "Music Mode", false);
    addBool (pid::musicLock, "Sample Lock", false);
    addBool (pid::musicKeepLength, "Keep Length", true);

    addBool (pid::fxOn, "FX Enabled", true);

    addBool (pid::chaosOn, "Chaos On", false);
    addFloat (pid::chaosPitch,   "Chaos Pitch",   percent, 15.0f, percentAttributes());
    addFloat (pid::chaosReverse, "Chaos Reverse", percent, 25.0f, percentAttributes());
    addFloat (pid::chaosPan,     "Chaos Pan",     percent, 60.0f, percentAttributes());

    addBool (pid::stutterOn, "Stutter On", false);
    addChoice (pid::stutterRate, "Stutter Rate", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" }, 3);
    addFloat (pid::stutterChance, "Stutter Chance", percent, 50.0f, percentAttributes());

    addBool (pid::dirtOn, "Dirt On", false);
    addFloat (pid::dirtDrive, "Dirt Drive",      { 0.0f, 36.0f },         12.0f, unitAttributes ("dB", 1));
    addFloat (pid::dirtBits,  "Dirt Bits",       { 2.0f, 24.0f, 0.1f },   10.0f, unitAttributes ("bit", 1));
    addFloat (pid::dirtRate,  "Dirt Downsample", skewed (1.0f, 32.0f, 6.0f), 2.0f, unitAttributes ("x", 1));

    addBool (pid::echoOn, "Echo On", false);
    addChoice (pid::echoTime, "Echo Time", { "1/32", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4", "1/4D", "1/2" }, 4);
    addFloat (pid::echoFeedback, "Echo Feedback", { 0.0f, 95.0f }, 40.0f, percentAttributes());
    addFloat (pid::echoMix,      "Echo Mix",      percent,         30.0f, percentAttributes());

    addBool (pid::reverbOn, "Reverb On", false);
    addFloat (pid::reverbMix,  "Reverb Mix",  percent, 25.0f, percentAttributes());
    addFloat (pid::reverbSize, "Reverb Size", percent, 60.0f, percentAttributes());
    addFloat (pid::reverbDamp, "Reverb Damp", percent, 40.0f, percentAttributes());

    return layout;
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Clean Sampler", {} },
        { "Cassette", { { pid::dirtOn, 1 }, { pid::dirtDrive, 6 }, { pid::dirtBits, 12 }, { pid::dirtRate, 2 },
                        { pid::cutoff, 9000 }, { pid::reverbOn, 1 }, { pid::reverbMix, 12 }, { pid::reverbSize, 35 } } },
        { "Basement", { { pid::cutoff, 900 }, { pid::resonance, 25 }, { pid::reverbOn, 1 }, { pid::reverbMix, 45 },
                        { pid::reverbSize, 85 }, { pid::reverbDamp, 70 }, { pid::echoOn, 1 }, { pid::echoTime, 5 },
                        { pid::echoFeedback, 35 }, { pid::echoMix, 18 } } },
        { "Stutter", { { pid::stutterOn, 1 }, { pid::stutterRate, 3 }, { pid::stutterChance, 65 },
                       { pid::echoOn, 1 }, { pid::echoTime, 4 }, { pid::echoFeedback, 35 }, { pid::echoMix, 22 } } },
        { "Chaos", { { pid::chaosOn, 1 }, { pid::chaosPitch, 35 }, { pid::chaosReverse, 40 }, { pid::chaosPan, 80 },
                     { pid::reverbOn, 1 }, { pid::reverbMix, 18 } } },
        { "Back to Space", { { pid::reverse, 1 }, { pid::attack, 30 }, { pid::release, 900 },
                             { pid::reverbOn, 1 }, { pid::reverbMix, 55 }, { pid::reverbSize, 95 },
                             { pid::echoOn, 1 }, { pid::echoFeedback, 55 }, { pid::echoMix, 30 } } },
        { "ZHRI", { { pid::dirtOn, 1 }, { pid::dirtDrive, 30 }, { pid::dirtBits, 5 }, { pid::dirtRate, 8 },
                    { pid::stutterOn, 1 }, { pid::stutterRate, 5 }, { pid::stutterChance, 45 },
                    { pid::chaosOn, 1 }, { pid::chaosPitch, 0 }, { pid::chaosReverse, 0 }, { pid::chaosPan, 60 },
                    { pid::gain, -6 } } },
    };
    return presets;
}
}
