#pragma once

#include "ArtStore.h"
#include "Parameters.h"
#include "SampleLibrary.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace rygo::ui
{
using Apvts = juce::AudioProcessorValueTreeState;

juce::String secondsText (double seconds);

//==============================================================================
class RygoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RygoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

private:
    juce::SharedResourcePointer<ArtStore> art;
};

//==============================================================================
/** A rotary control: drag with the mouse, click the number below it to type a value. */
class Knob final : public juce::Component
{
public:
    Knob (Apvts& state, const char* paramId, const char* labelImage, const juce::String& labelText, const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::SharedResourcePointer<ArtStore> art;
    juce::String labelImage, labelText;
    std::unique_ptr<Apvts::SliderAttachment> attachment;
};

//==============================================================================
/** Choices that are all visible at once; the current one is circled by hand. */
class SegmentedChoice final : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Option { const char* image; juce::String text; };

    SegmentedChoice (juce::RangedAudioParameter& parameter, std::vector<Option> options, bool stackVertically, const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> areaFor (int index) const;

    juce::SharedResourcePointer<ArtStore> art;
    std::vector<Option> options;
    bool vertical;
    juce::ParameterAttachment attachment;
    int current = 0;
};

//==============================================================================
/** An on/off switch: a hand-drawn indicator next to a hand-drawn label. */
class ArtToggle final : public juce::Button
{
public:
    ArtToggle (Apvts& state, const char* paramId, const char* image, const juce::String& text, const juce::String& tooltip);

    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;

private:
    juce::SharedResourcePointer<ArtStore> art;
    juce::String image, text;
    std::unique_ptr<Apvts::ButtonAttachment> attachment;
};

//==============================================================================
class ArtButton final : public juce::Button
{
public:
    ArtButton (const char* image, const juce::String& text, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;

private:
    juce::SharedResourcePointer<ArtStore> art;
    juce::String image, text;
};

//==============================================================================
/** The Sample Lock: a hand-drawn padlock, open or closed. */
class LockButton final : public juce::Button
{
public:
    LockButton (Apvts& state, const char* paramId, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;

private:
    juce::SharedResourcePointer<ArtStore> art;
    std::unique_ptr<Apvts::ButtonAttachment> attachment;
};

//==============================================================================
/** A group of knobs under one title. With a power parameter, the title is its on/off switch. */
class Section final : public juce::Component
{
public:
    Section (const char* titleImage, const juce::String& titleText);
    Section (const char* titleImage, const juce::String& titleText, Apvts& state, const char* powerParamId, const juce::String& tooltip);

    Knob& addKnob (Apvts& state, const char* paramId, const char* labelImage, const juce::String& labelText, const juce::String& tooltip);

    /** The space left over to the right of the knobs, for buttons and choices. */
    juce::Rectangle<int> getFreeArea() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateDimming();

    static constexpr int knobWidth = 62;
    static constexpr int padding = 6;
    static constexpr int titleHeight = 24;

private:
    juce::SharedResourcePointer<ArtStore> art;
    juce::String titleImage, titleText;
    juce::OwnedArray<Knob> knobs;
    std::unique_ptr<ArtToggle> power;
};

//==============================================================================
class WaveformView final : public juce::Component
{
public:
    void show (BankPtr bank, const SampleData* sample, int note, float playhead, const juce::String& status);
    void paint (juce::Graphics&) override;

private:
    juce::SharedResourcePointer<ArtStore> art;
    BankPtr bank;
    const SampleData* sample = nullptr;
    int note = -1;
    float playhead = -1.0f;
    juce::String status;
};

//==============================================================================
/** The keyboard, tinted by sample length: dark keys hold long samples, pale keys short ones. */
class KeyMap final : public juce::MidiKeyboardComponent
{
public:
    explicit KeyMap (juce::MidiKeyboardState&);

    void setBank (BankPtr newBank);
    void resized() override;

    /** Locked: every key plays one sample. The marked key is the chosen sample's, or its root once locked. */
    void setMusicState (bool isLocked, int markedNote);
    bool isSampleLocked() const noexcept { return locked; }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (int)> onHover;
    float lengthAmount (int note) const;

protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;

private:
    void fitKeys();

    juce::SharedResourcePointer<ArtStore> art;
    BankPtr bank;
    double longest = 1.0;
    bool locked = false;
    int marked = -1;
};

//==============================================================================
/** A bar right above each key: the staircase from long to short. */
class LengthStrip final : public juce::Component
{
public:
    LengthStrip (KeyMap& keys, juce::MidiKeyboardState& state);
    void paint (juce::Graphics&) override;

private:
    KeyMap& keys;
    juce::MidiKeyboardState& keyboardState;
};

}
