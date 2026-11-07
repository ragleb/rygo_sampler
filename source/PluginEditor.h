#pragma once

#include "PluginProcessor.h"
#include "UiComponents.h"

namespace rygo
{
/** Positions of everything on the panel, in points at 100% scale. */
namespace layout
{
    inline constexpr int width = 1180, height = 760;

    inline const juce::Rectangle<int> logo     {   24,   8, 400,  84 };
    inline const juce::Rectangle<int> presets  {  456,  14, 330,  72 };
    inline const juce::Rectangle<int> fxMaster {  810,  36, 150,  28 };
    inline const juce::Rectangle<int> zhri     {  990,  18, 170,  64 };

    inline const juce::Rectangle<int> music    {   16, 104, 180, 416 };
    inline const juce::Rectangle<int> wave     {  208, 104, 956, 120 };

    inline const juce::Rectangle<int> envelope {  208, 230, 322, 142 };
    inline const juce::Rectangle<int> filter   {  538, 230, 250, 142 };
    inline const juce::Rectangle<int> sample   {  796, 230, 368, 142 };

    inline const juce::Rectangle<int> chaos    {  208, 378, 198, 142 };
    inline const juce::Rectangle<int> stutter  {  413, 378, 136, 142 };
    inline const juce::Rectangle<int> dirt     {  556, 378, 198, 142 };
    inline const juce::Rectangle<int> echo     {  761, 378, 198, 142 };
    inline const juce::Rectangle<int> reverb   {  966, 378, 198, 142 };

    inline const juce::Rectangle<int> strip    {   16, 526, 1148, 28 };
    inline const juce::Rectangle<int> keyboard {   16, 554, 1148, 168 };
    inline const juce::Rectangle<int> footer   {   16, 726, 1148, 26 };
}

class RygoEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RygoEditor (RygoProcessor&);
    ~RygoEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    class Content;

    RygoProcessor& rygoProcessor;
    ui::RygoLookAndFeel lookAndFeel;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 700 };
    bool isConstructed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RygoEditor)
};
}
