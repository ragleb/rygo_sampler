#include "UiComponents.h"

namespace rygo::ui
{
juce::String secondsText (double seconds)
{
    return juce::String (seconds, 2) + " s";
}

//==============================================================================
namespace
{
class ValueLabel final : public juce::Label
{
public:
    void mouseEnter (const juce::MouseEvent&) override
    {
        setColour (backgroundColourId, pal::kraftLight.withAlpha (0.7f));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setColour (backgroundColourId, juce::Colours::transparentBlack);
    }
};
}

RygoLookAndFeel::RygoLookAndFeel()
{
    using namespace juce;

    setColour (Slider::textBoxTextColourId, pal::ink);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, pal::ink.withAlpha (0.25f));

    setColour (Label::textColourId, pal::ink);
    setColour (Label::textWhenEditingColourId, pal::ink);
    setColour (Label::backgroundWhenEditingColourId, pal::kraftLight);
    setColour (Label::outlineWhenEditingColourId, pal::ink);

    setColour (TextEditor::backgroundColourId, pal::kraftLight);
    setColour (TextEditor::textColourId, pal::ink);
    setColour (TextEditor::highlightColourId, pal::ink.withAlpha (0.25f));
    setColour (TextEditor::highlightedTextColourId, pal::ink);
    setColour (TextEditor::outlineColourId, pal::ink);
    setColour (TextEditor::focusedOutlineColourId, pal::ink);
    setColour (CaretComponent::caretColourId, pal::ink);

    setColour (ComboBox::textColourId, pal::ink);
    setColour (ComboBox::backgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (ComboBox::arrowColourId, pal::ink);

    setColour (PopupMenu::backgroundColourId, pal::kraftLight);
    setColour (PopupMenu::textColourId, pal::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, pal::ink);
    setColour (PopupMenu::highlightedTextColourId, pal::kraftLight);

    setColour (TooltipWindow::backgroundColourId, pal::kraftLight);
    setColour (TooltipWindow::textColourId, pal::ink);
    setColour (TooltipWindow::outlineColourId, pal::ink);

    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (ListBox::outlineColourId, Colours::transparentBlack);
    setColour (ScrollBar::thumbColourId, pal::ink.withAlpha (0.45f));
    setColour (ScrollBar::backgroundColourId, Colours::transparentBlack);
}

void RygoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto size = (float) juce::jmin (width, height) - 4.0f;
    const auto area = juce::Rectangle<float> (size, size).withCentre ({ (float) x + (float) width * 0.5f,
                                                                        (float) y + (float) height * 0.5f });
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto centre = area.getCentre();

    const auto base = art->image ("knob_base");
    if (base.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (base, area, juce::RectanglePlacement::centred);
    }
    else
    {
        g.setColour (pal::kraftLight);
        g.fillEllipse (area);
        g.setColour (pal::ink);
        g.drawEllipse (area, 1.5f);
    }

    const auto pointer = art->image ("knob_pointer");
    if (pointer.isValid())
    {
        const auto scale = size / (float) pointer.getWidth();
        g.drawImageTransformed (pointer, juce::AffineTransform::translation (-(float) pointer.getWidth() * 0.5f,
                                                                             -(float) pointer.getHeight() * 0.5f)
                                             .scaled (scale)
                                             .rotated (angle)
                                             .translated (centre));
    }
    else
    {
        juce::Path line;
        line.addLineSegment ({ 0.0f, -size * 0.2f, 0.0f, -size * 0.44f }, 2.5f);
        g.setColour (pal::ink);
        g.fillPath (line, juce::AffineTransform::rotation (angle).translated (centre));
    }
}

juce::Label* RygoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = new ValueLabel();
    label->setJustificationType (juce::Justification::centred);
    label->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
    label->setColour (juce::Label::textColourId, slider.findColour (juce::Slider::textBoxTextColourId));
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::textColourId, pal::ink);
    label->setColour (juce::TextEditor::backgroundColourId, pal::kraftLight);
    label->setColour (juce::TextEditor::highlightColourId, pal::ink.withAlpha (0.25f));
    label->setFont (art->font (12.0f));
    label->setMinimumHorizontalScale (0.7f);
    label->setMouseCursor (juce::MouseCursor::IBeamCursor);
    return label;
}

void RygoLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    g.setColour (pal::ink);
    g.fillRect (2.0f, (float) height - 2.0f, (float) width - 4.0f, 1.5f);
}

void RygoLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (2, 0));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

juce::Font RygoLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    // The note names are what the Music panel is about.
    const auto isNote = box.getComponentID() == "sampleBox" || box.getComponentID() == "rootBox";
    return art->font (isNote ? 22.0f : 15.0f, true);
}

juce::Font RygoLookAndFeel::getPopupMenuFont()
{
    return art->font (15.0f);
}

void RygoLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (pal::kraftLight);
    g.setColour (pal::ink);
    g.drawRect (0, 0, width, height, 1);
}

//==============================================================================
Knob::Knob (Apvts& state, const char* paramId, const char* image, const juce::String& text, const juce::String& tooltip)
    : labelImage (image), labelText (text)
{
    setComponentID (paramId);

    slider.setName (paramId);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.setVelocityModeParameters (0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // Shift: fine adjustment
    slider.setTooltip (tooltip);
    slider.setWantsKeyboardFocus (false);
    addAndMakeVisible (slider);

    attachment = std::make_unique<Apvts::SliderAttachment> (state, paramId, slider);

    if (auto* parameter = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
}

void Knob::paint (juce::Graphics& g)
{
    art->drawArt (g, labelImage, getLocalBounds().removeFromTop (17).toFloat(), labelText);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (17));
}

//==============================================================================
SegmentedChoice::SegmentedChoice (juce::RangedAudioParameter& parameter, std::vector<Option> newOptions,
                                  bool stackVertically, const juce::String& tooltip)
    : options (std::move (newOptions)),
      vertical (stackVertically),
      attachment (parameter, [this] (float value)
      {
          current = juce::roundToInt (value);
          repaint();
      })
{
    setTooltip (tooltip);
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> SegmentedChoice::areaFor (int index) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto count = (float) options.size();

    if (vertical)
    {
        const auto h = bounds.getHeight() / count;
        return { 0.0f, (float) index * h, bounds.getWidth(), h };
    }

    const auto w = bounds.getWidth() / count;
    return { (float) index * w, 0.0f, w, bounds.getHeight() };
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    for (int i = 0; i < (int) options.size(); ++i)
    {
        const auto area = areaFor (i);
        if (i == current)
            art->drawArt (g, "mark_selected", area, {}, juce::RectanglePlacement::stretchToFit);

        art->drawArt (g, options[(size_t) i].image, area.reduced (5.0f, 3.0f), options[(size_t) i].text);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < (int) options.size(); ++i)
        if (areaFor (i).contains (event.position))
            attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
ArtToggle::ArtToggle (Apvts& state, const char* paramId, const char* imageName, const juce::String& labelText, const juce::String& tooltip)
    : juce::Button (paramId), image (imageName), text (labelText)
{
    setComponentID (paramId);
    setClickingTogglesState (true);
    setTooltip (tooltip);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment = std::make_unique<Apvts::ButtonAttachment> (state, paramId, *this);
}

void ArtToggle::paintButton (juce::Graphics& g, bool isOver, bool)
{
    auto area = getLocalBounds().toFloat();
    const auto ledSize = juce::jmin (15.0f, area.getHeight());

    const auto led = area.removeFromLeft (ledSize + 5.0f).withSizeKeepingCentre (ledSize, ledSize);
    if (getToggleState())
    {
        art->drawArt (g, "led_on", led, {});
    }
    else
    {
        art->drawArt (g, "led_off", led, {});
    }

    g.setOpacity (isOver ? 0.7f : 1.0f);
    art->drawArt (g, image, area, text, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
}

//==============================================================================
ArtButton::ArtButton (const char* imageName, const juce::String& labelText, const juce::String& tooltip)
    : juce::Button (imageName), image (imageName), text (labelText)
{
    setComponentID (imageName);
    setTooltip (tooltip);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ArtButton::paintButton (juce::Graphics& g, bool isOver, bool isDown)
{
    auto area = getLocalBounds().toFloat();
    if (isDown)
        area = area.translated (0.0f, 1.0f);
    if (isOver)
        area = area.expanded (1.0f);

    art->drawArt (g, image, area, text);
}

//==============================================================================
LockButton::LockButton (Apvts& state, const char* paramId, const juce::String& tooltip)
    : juce::Button (paramId)
{
    setComponentID (paramId);
    setClickingTogglesState (true);
    setTooltip (tooltip);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment = std::make_unique<Apvts::ButtonAttachment> (state, paramId, *this);
}

void LockButton::paintButton (juce::Graphics& g, bool isOver, bool isDown)
{
    auto area = getLocalBounds().toFloat();
    if (isDown)
        area = area.translated (0.0f, 1.0f);
    if (isOver && isEnabled())
        area = area.expanded (1.0f);

    const auto isClosed = getToggleState();
    art->drawArt (g, isClosed ? "lock_closed" : "lock_open", area, isClosed ? "LOCKED" : "OPEN");
}

//==============================================================================
Section::Section (const char* image, const juce::String& text)
    : titleImage (image), titleText (text)
{
}

Section::Section (const char* image, const juce::String& text, Apvts& state, const char* powerParamId, const juce::String& tooltip)
    : titleImage (image), titleText (text)
{
    power = std::make_unique<ArtToggle> (state, powerParamId, image, text, tooltip);
    power->onStateChange = [this] { updateDimming(); };
    addAndMakeVisible (*power);
}

Knob& Section::addKnob (Apvts& state, const char* paramId, const char* labelImage, const juce::String& labelText, const juce::String& tooltip)
{
    auto* knob = knobs.add (new Knob (state, paramId, labelImage, labelText, tooltip));
    addAndMakeVisible (knob);
    return *knob;
}

juce::Rectangle<int> Section::getFreeArea() const
{
    return getLocalBounds().withTrimmedTop (titleHeight)
                           .withTrimmedLeft (padding + knobs.size() * knobWidth)
                           .withTrimmedRight (padding);
}

void Section::paint (juce::Graphics& g)
{
    if (power == nullptr)
        art->drawArt (g, titleImage, getLocalBounds().removeFromTop (titleHeight).reduced (padding, 2).toFloat(), titleText,
                      juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
}

void Section::resized()
{
    auto area = getLocalBounds();
    const auto titleRow = area.removeFromTop (titleHeight);

    if (power != nullptr)
        power->setBounds (titleRow.reduced (padding, 2));

    area.removeFromLeft (padding);
    area.removeFromBottom (2);

    for (auto* knob : knobs)
        knob->setBounds (area.removeFromLeft (knobWidth));

    updateDimming();
}

void Section::updateDimming()
{
    if (power == nullptr)
        return;

    const auto alpha = power->getToggleState() ? 1.0f : 0.55f;
    for (auto* knob : knobs)
        knob->setAlpha (alpha);
}

//==============================================================================
void WaveformView::show (BankPtr newBank, const SampleData* newSample, int newNote, float newPlayhead, const juce::String& newStatus)
{
    const auto changed = newSample != sample || newNote != note || newStatus != status
                      || ! juce::approximatelyEqual (newPlayhead, playhead);

    bank = std::move (newBank);
    sample = newSample;
    note = newNote;
    playhead = newPlayhead;
    status = newStatus;

    if (changed)
        repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (14.0f, 8.0f);
    auto header = area.removeFromTop (20.0f);

    g.setColour (pal::ink);

    if (status.isNotEmpty())
    {
        g.setFont (art->font (16.0f));
        g.drawFittedText (status, getLocalBounds().reduced (16), juce::Justification::centred, 3);
        return;
    }

    if (sample == nullptr)
    {
        g.setFont (art->font (16.0f));
        g.drawFittedText ("Play a key or point at one to see its sample", getLocalBounds().reduced (16), juce::Justification::centred, 1);
        return;
    }

    const auto noteName = juce::MidiMessage::getMidiNoteName (note, true, true, 3);

    g.setFont (art->font (15.0f, true));
    g.drawText (noteName + "  " + sample->name, header, juce::Justification::centredLeft);

    g.setFont (art->font (13.0f));
    g.drawText ("audible " + secondsText (sample->audibleSeconds) + "   file " + secondsText (sample->fileSeconds),
                header, juce::Justification::centredRight);

    const auto bins = (int) sample->overview.size();
    const auto total = (float) sample->audio.getNumSamples();
    const auto mid = area.getCentreY();
    const auto scale = area.getHeight() * 0.5f / juce::jmax (sample->peak, 0.0001f);
    const auto barWidth = area.getWidth() / (float) bins;

    for (int bin = 0; bin < bins; ++bin)
    {
        const auto fraction = (float) bin / (float) bins;
        const auto frame = fraction * total;
        const auto isAudible = frame >= (float) sample->trimStart && frame <= (float) sample->trimEnd;
        const auto height = juce::jmax (0.6f, sample->overview[(size_t) bin] * scale);

        g.setColour (pal::ink.withAlpha (isAudible ? 1.0f : 0.25f));
        g.fillRect (area.getX() + (float) bin * barWidth, mid - height, juce::jmax (1.0f, barWidth - 0.5f), height * 2.0f);
    }

    if (playhead >= 0.0f)
    {
        g.setColour (pal::hot);
        g.fillRect (area.getX() + playhead * area.getWidth() - 1.0f, area.getY(), 2.0f, area.getHeight());
    }
}

//==============================================================================
KeyMap::KeyMap (juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent (state, juce::KeyboardComponentBase::horizontalKeyboard)
{
    setOctaveForMiddleC (3);
    setScrollButtonsVisible (false);
    setWantsKeyboardFocus (false);
    clearKeyMappings();
}

void KeyMap::setBank (BankPtr newBank)
{
    bank = std::move (newBank);
    longest = 1.0;

    int low = 36, high = 83;

    if (bank != nullptr && ! bank->samples.empty())
    {
        longest = juce::jmax (0.001, bank->samples.front()->audibleSeconds);
        low = bank->firstNote;
        high = low + (int) bank->samples.size() - 1;
        low -= low % 12;
        high = juce::jmin (127, high + (11 - high % 12));
    }

    setAvailableRange (low, high);
    fitKeys();
    repaint();
}

void KeyMap::fitKeys()
{
    int whiteKeys = 0;
    for (int note = getRangeStart(); note <= getRangeEnd(); ++note)
        if (! juce::MidiMessage::isMidiNoteBlack (note))
            ++whiteKeys;

    setKeyWidth ((float) getWidth() / (float) juce::jmax (1, whiteKeys));
    setLowestVisibleKey (getRangeStart());
}

void KeyMap::resized()
{
    fitKeys();
    juce::MidiKeyboardComponent::resized();
}

void KeyMap::mouseMove (const juce::MouseEvent& event)
{
    juce::MidiKeyboardComponent::mouseMove (event);
    if (onHover)
        onHover (getNoteAndVelocityAtPosition (event.position).note);
}

void KeyMap::mouseExit (const juce::MouseEvent& event)
{
    juce::MidiKeyboardComponent::mouseExit (event);
    if (onHover)
        onHover (-1);
}

void KeyMap::setMusicState (bool isLocked, int markedNote)
{
    if (isLocked == locked && markedNote == marked)
        return;

    locked = isLocked;
    marked = markedNote;
    repaint();
}

float KeyMap::lengthAmount (int note) const
{
    if (locked)
        return 0.5f;   // one sample on every key: no key is longer than another

    const auto* sample = bank != nullptr ? bank->sampleForNote (note) : nullptr;
    if (sample == nullptr)
        return -1.0f;

    // Logarithmic: the audible difference between 0.1 s and 0.5 s matters as much as between 1 s and 5 s.
    return (float) (std::log1p (sample->audibleSeconds * 10.0) / std::log1p (longest * 10.0));
}

void KeyMap::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool,
                            juce::Colour, juce::Colour)
{
    const auto amount = lengthAmount (note);
    auto colour = amount < 0.0f ? pal::kraftDark : pal::kraftLight.interpolatedWith (pal::kraft.darker (0.25f), amount);
    if (isDown)
        colour = pal::hot;

    g.setColour (colour);
    g.fillRect (area);
    g.setColour (pal::ink);
    g.fillRect (area.withLeft (area.getRight() - 1.0f));

    if (note == marked)
    {
        const auto size = juce::jmin (9.0f, area.getWidth() - 3.0f);
        g.setColour (pal::hot);
        g.fillEllipse (area.getCentreX() - size * 0.5f, area.getBottom() - 34.0f, size, size);
    }

    const auto text = getWhiteNoteText (note);
    if (text.isNotEmpty())
    {
        g.setColour (pal::ink);
        g.setFont (art->font (10.5f));
        g.drawFittedText (text, area.withTrimmedBottom (3.0f).toNearestInt(), juce::Justification::centredBottom, 1, 0.5f);
    }
}

void KeyMap::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool, juce::Colour)
{
    const auto amount = lengthAmount (note);
    auto colour = amount < 0.0f ? pal::ink.withMultipliedAlpha (0.6f) : pal::ink.brighter (0.7f).interpolatedWith (pal::ink, amount);
    if (isDown)
        colour = pal::hot;

    g.setColour (colour);
    g.fillRect (area);

    if (note == marked)
    {
        const auto size = juce::jmin (8.0f, area.getWidth() - 1.0f);
        g.setColour (pal::hot);
        g.fillEllipse (area.getCentreX() - size * 0.5f, area.getBottom() - size - 6.0f, size, size);
    }
}

//==============================================================================
LengthStrip::LengthStrip (KeyMap& keyMap, juce::MidiKeyboardState& state)
    : keys (keyMap), keyboardState (state)
{
    setInterceptsMouseClicks (false, false);
}

void LengthStrip::paint (juce::Graphics& g)
{
    if (keys.isSampleLocked())
        return;

    const auto height = (float) getHeight();

    for (int note = keys.getRangeStart(); note <= keys.getRangeEnd(); ++note)
    {
        const auto amount = keys.lengthAmount (note);
        if (amount < 0.0f)
            continue;

        const auto isBlack = juce::MidiMessage::isMidiNoteBlack (note);
        const auto width = keys.getKeyWidth() * (isBlack ? keys.getBlackNoteWidthProportion() : 1.0f);
        const auto x = keys.getKeyStartPosition (note);
        const auto barHeight = juce::jmax (1.5f, amount * (height - 1.0f));

        g.setColour (keyboardState.isNoteOnForChannels (0xffff, note) ? pal::hot : pal::ink.withAlpha (isBlack ? 0.45f : 0.85f));
        g.fillRect (x + 0.5f, height - barHeight, juce::jmax (1.0f, width - 1.0f), barHeight);
    }
}

}
