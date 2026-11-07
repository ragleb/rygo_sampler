#include "PluginEditor.h"

namespace rygo
{
using namespace ui;

namespace
{
juce::String noteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}
}

//==============================================================================
/** Music mode: pick a sample by playing it or from the list, then lock it to play it across the keyboard. */
class MusicPanel final : public juce::Component
{
public:
    explicit MusicPanel (RygoProcessor& p)
        : proc (p),
          power (p.getState(), pid::musicOn, "title_music", "MUSIC",
                 "Music mode: pick one sample and play it in tune across the whole keyboard"),
          lock (p.getState(), pid::musicLock, "Sample Lock: closed keeps this sample on every key, pitched from its root note. Open goes back to choosing"),
          keepLength (p.getState(), pid::musicKeepLength, "opt_keep_length", "KEEP LENGTH",
                      "On: changing the pitch keeps the sample's length. Off: higher notes play shorter, lower notes longer, like a tape")
    {
        setComponentID ("musicPanel");

        sampleBox.setComponentID ("sampleBox");
        sampleBox.setWantsKeyboardFocus (false);
        sampleBox.setTooltip ("The chosen sample, named after its key. It follows the last note you played, or pick one here");
        sampleBox.onChange = [this]
        {
            const auto id = sampleBox.getSelectedId();
            if (id > 0)
                proc.selectNote (id - 1);
        };

        rootBox.setComponentID ("rootBox");
        rootBox.setWantsKeyboardFocus (false);
        rootBox.setTooltip ("The note this sample sounds closest to: that key plays it unchanged. Detected automatically, or set it here");
        rootBox.onChange = [this]
        {
            const auto id = rootBox.getSelectedId();
            if (id == autoRootId)
                proc.setRootOverride (-1);
            else if (id > 0)
                proc.setRootOverride (id - 1);
        };

        rootBox.addItem ("Auto", autoRootId);
        for (int note = 0; note < 128; ++note)
            rootBox.addItem (noteName (note), note + 1);

        for (auto* child : std::initializer_list<juce::Component*> { &power, &sampleBox, &lock, &rootBox, &keepLength })
            addAndMakeVisible (child);
    }

    void paint (juce::Graphics& g) override
    {
        const auto drawLabel = [&] (const char* image, const juce::String& text, juce::Rectangle<int> area)
        {
            art->drawArt (g, image, area.toFloat(), text);
        };

        juce::Graphics::ScopedSaveState saved (g);
        if (! isOn)
            g.setOpacity (0.45f);

        drawLabel ("label_sample", "SAMPLE", sampleLabelArea);
        drawLabel ("label_lock", "SAMPLE LOCK", lockLabelArea);
        drawLabel ("label_root", "ROOT", rootLabelArea);

        g.setColour (pal::ink.withAlpha (isOn ? 1.0f : 0.45f));
        g.setFont (art->font (12.5f));
        g.drawFittedText (sampleInfo, sampleInfoArea, juce::Justification::centred, 2, 0.8f);
        g.drawFittedText (rootInfo, rootInfoArea, juce::Justification::centred, 1, 0.8f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 6);

        power.setBounds (area.removeFromTop (24));
        area.removeFromTop (8);

        sampleLabelArea = area.removeFromTop (17);
        sampleBox.setBounds (area.removeFromTop (32));
        sampleInfoArea = area.removeFromTop (34);
        area.removeFromTop (6);

        lock.setBounds (area.removeFromTop (80).withSizeKeepingCentre (80, 80));
        lockLabelArea = area.removeFromTop (20);
        area.removeFromTop (12);

        rootLabelArea = area.removeFromTop (17);
        rootBox.setBounds (area.removeFromTop (32));
        rootInfoArea = area.removeFromTop (20);
        area.removeFromTop (10);

        keepLength.setBounds (area.removeFromTop (24));
    }

    void setBank (const BankPtr& newBank)
    {
        bank = newBank;
        sampleBox.clear (juce::dontSendNotification);

        if (bank != nullptr)
            for (int note = 0; note < 128; ++note)
                if (const auto* sample = bank->sampleForNote (note))
                    sampleBox.addItem (noteName (note) + "   " + sample->name, note + 1);
    }

    void update()
    {
        const auto musicOn = proc.getState().getRawParameterValue (pid::musicOn)->load() > 0.5f;
        const auto locked = proc.isSampleLocked();
        const auto selected = proc.getSelectedNote();
        const auto override = proc.getRootOverride();
        const auto* sample = bank != nullptr ? bank->sampleForNote (selected) : nullptr;

        // Only the switch itself stays alive while the section is off.
        lock.setEnabled (musicOn);
        rootBox.setEnabled (musicOn);
        keepLength.setEnabled (musicOn);
        sampleBox.setEnabled (musicOn && ! locked);

        const auto dim = [] (juce::Component& component, bool isActive) { component.setAlpha (isActive ? 1.0f : 0.45f); };
        dim (lock, musicOn);
        dim (rootBox, musicOn);
        dim (keepLength, musicOn);
        dim (sampleBox, musicOn && ! locked);

        const auto sampleText = sample != nullptr ? noteName (selected) : juce::String ("-");
        if (sampleBox.getText() != sampleText)
            sampleBox.setText (sampleText, juce::dontSendNotification);

        juce::String newRootText = "-", newRootInfo, newSampleInfo;

        if (sample != nullptr)
        {
            newRootText = noteName (SamplerEngine::effectiveRoot (*sample, override));
            newRootInfo = override >= 0 ? "set by hand" : sample->hasClearPitch ? "detected" : "estimated, noisy sound";
            newSampleInfo = sample->name + "\n" + secondsText (sample->audibleSeconds);
        }

        if (rootBox.getText() != newRootText)
            rootBox.setText (newRootText, juce::dontSendNotification);

        if (newRootInfo != rootInfo || newSampleInfo != sampleInfo || musicOn != isOn)
        {
            rootInfo = newRootInfo;
            sampleInfo = newSampleInfo;
            isOn = musicOn;
            repaint();
        }
    }

private:
    static constexpr int autoRootId = 1000;

    RygoProcessor& proc;
    juce::SharedResourcePointer<ArtStore> art;
    BankPtr bank;

    ArtToggle power;
    juce::ComboBox sampleBox, rootBox;
    LockButton lock;
    ArtToggle keepLength;

    juce::Rectangle<int> sampleLabelArea, sampleInfoArea, lockLabelArea, rootLabelArea, rootInfoArea;
    juce::String sampleInfo, rootInfo;
    bool isOn = false;
};

//==============================================================================
class RygoEditor::Content final : public juce::Component
{
public:
    explicit Content (RygoProcessor& p)
        : proc (p),
          state (p.getState()),
          music (p),
          keys (p.getKeyboardState()),
          strip (keys, p.getKeyboardState()),
          envelope ("title_envelope", "ENVELOPE"),
          filter ("title_filter", "FILTER"),
          samplePanel ("title_sample", "SAMPLE"),
          chaos ("title_chaos", "CHAOS", state, pid::chaosOn, "Gives every note its own random pitch, direction and pan"),
          stutter ("title_stutter", "STUTTER", state, pid::stutterOn, "Randomly loops the slice that just played, in time with the song"),
          dirt ("title_dirt", "DIRT", state, pid::dirtOn, "Overdrive and digital degradation"),
          echo ("title_echo", "ECHO", state, pid::echoOn, "Echo in time with the song, bouncing between left and right"),
          reverb ("title_reverb", "REVERB", state, pid::reverbOn, "Reverb"),
          filterType (*state.getParameter (pid::filterType),
                      { { "opt_lp", "LP" }, { "opt_hp", "HP" }, { "opt_bp", "BP" } }, true,
                      "Filter type: LP cuts the highs, HP cuts the lows, BP keeps the middle"),
          mode (*state.getParameter (pid::mode),
                { { "opt_oneshot", "ONE SHOT" }, { "opt_gate", "GATE" }, { "opt_loop", "LOOP" } }, false,
                "ONE SHOT: always plays to the end. GATE: plays while the key is held. LOOP: starts again from the beginning while the key is held"),
          reverse (state, pid::reverse, "opt_reverse", "REVERSE", "Play the samples backwards"),
          trim (state, pid::trimSilence, "opt_trim", "TRIM SILENCE", "Cut the silence at the start and end of each file, so notes sound exactly when played"),
          normalize (state, pid::normalize, "opt_normalize", "NORMALIZE", "Bring all samples to the same peak level"),
          fxMaster (state, pid::fxOn, "title_fx", "EFFECTS", "Turn off to hear the plain samples: the plugin becomes a simple sampler"),
          prevPreset ("arrow_prev", "<", "Previous preset"),
          nextPreset ("arrow_next", ">", "Next preset")
    {
        addKnobs();

        for (auto* section : { &envelope, &filter, &samplePanel, &chaos, &stutter, &dirt, &echo, &reverb })
            addAndMakeVisible (section);

        filter.addAndMakeVisible (filterType);
        samplePanel.addAndMakeVisible (mode);
        samplePanel.addAndMakeVisible (reverse);
        samplePanel.addAndMakeVisible (trim);
        samplePanel.addAndMakeVisible (normalize);

        addAndMakeVisible (fxMaster);
        addAndMakeVisible (waveform);
        addAndMakeVisible (music);
        addAndMakeVisible (strip);
        addAndMakeVisible (keys);

        setupPresets();

        keys.onHover = [this] (int note) { hoverNote = note; };

        setBounds (0, 0, layout::width, layout::height);
    }

    void paint (juce::Graphics& g) override
    {
        const auto background = art->image ("background");

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        if (background.isValid())
            g.drawImage (background, getLocalBounds().toFloat());
        else
            g.fillAll (pal::kraft);

        const auto lines = art->image ("panel_lines");
        if (lines.isValid())
            g.drawImage (lines, getLocalBounds().toFloat());

        art->drawArt (g, "logo_rygostancija", layout::logo.toFloat(), "RYGOSTANCIJA");
        art->drawArt (g, "logo_zhri", layout::zhri.toFloat(), "ZHRI Records");
        art->drawArt (g, "title_presets", layout::presets.withHeight (22).toFloat(), "PRESET");

        // With the lock closed every key plays the same sample, so length no longer lives on the keyboard.
        if (! isLocked)
        {
            auto hints = layout::footer.reduced (0, 2).toFloat();
            art->drawArt (g, "hint_long", hints.removeFromLeft (110.0f), "LONG");
            art->drawArt (g, "hint_short", hints.removeFromRight (110.0f), "SHORT");
        }

        g.setColour (pal::ink);
        g.setFont (art->font (12.5f));
        g.drawText ("Drag a knob with the mouse  |  Shift: fine control  |  Double-click: reset  |  Click a number: type a value",
                    layout::footer.reduced (130, 0), juce::Justification::centred, true);
    }

    void resized() override
    {
        const auto place = [] (juce::Component& component, juce::Rectangle<int> bounds) { component.setBounds (bounds); };

        place (music, layout::music);
        place (waveform, layout::wave);

        place (envelope, layout::envelope);
        place (filter, layout::filter);
        place (samplePanel, layout::sample);
        place (chaos, layout::chaos);
        place (stutter, layout::stutter);
        place (dirt, layout::dirt);
        place (echo, layout::echo);
        place (reverb, layout::reverb);

        filterType.setBounds (filter.getFreeArea().reduced (2, 6));

        // Three modes need more room than the column under them: they share the title row.
        mode.setBounds (samplePanel.getLocalBounds().removeFromTop (Section::titleHeight + 2).withTrimmedLeft (100).withTrimmedRight (6));

        auto options = samplePanel.getFreeArea().reduced (4, 6);
        const auto rowHeight = options.getHeight() / 3;
        reverse.setBounds (options.removeFromTop (rowHeight).reduced (2, 4));
        trim.setBounds (options.removeFromTop (rowHeight).reduced (2, 4));
        normalize.setBounds (options.reduced (2, 4));

        place (fxMaster, layout::fxMaster);

        auto presetRow = layout::presets.withTrimmedTop (24).withHeight (34);
        prevPreset.setBounds (presetRow.removeFromLeft (30).reduced (2));
        nextPreset.setBounds (presetRow.removeFromRight (30).reduced (2));
        presetBox.setBounds (presetRow.reduced (4, 2));

        place (strip, layout::strip);
        place (keys, layout::keyboard);
    }

    void update()
    {
        const auto bank = proc.getLoadedBank();
        const auto locked = proc.isSampleLocked();
        const auto musicOn = state.getRawParameterValue (pid::musicOn)->load() > 0.5f;
        const auto selected = proc.getSelectedNote();

        if (bank.get() != shownBank.get())
        {
            shownBank = bank;
            keys.setBank (bank);
            music.setBank (bank);
        }

        if (locked != isLocked)
        {
            isLocked = locked;
            repaint();
        }

        const auto* selectedSample = bank != nullptr ? bank->sampleForNote (selected) : nullptr;
        const auto root = selectedSample != nullptr ? SamplerEngine::effectiveRoot (*selectedSample, proc.getRootOverride()) : -1;

        // The mark shows the key that matters: the chosen sample's while choosing, the root once locked.
        keys.setMusicState (locked, locked ? root : (musicOn ? selected : -1));

        music.update();

        const auto engineNote = proc.getEngine().lastNote.load();
        const auto note = locked ? selected : (hoverNote >= 0 ? hoverNote : engineNote);
        const auto* sample = bank != nullptr ? bank->sampleForNote (note) : nullptr;
        const auto isHovering = ! locked && hoverNote >= 0 && hoverNote != engineNote;
        const auto playhead = isHovering ? -1.0f : proc.getEngine().lastPosition.load();

        juce::String status;
        if (proc.isBankLoading())
            status = "Loading samples...";
        else if (bank != nullptr && bank->folderMissing)
            status = bank->notes[0];
        else if (bank != nullptr && bank->samples.empty())
            status = "No samples found. Please reinstall the plugin.";

        waveform.show (bank, sample, note, playhead, status);
        strip.repaint();

        if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
            presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    }

private:
    void addKnobs()
    {
        envelope.addKnob (state, pid::attack, "label_attack", "ATTACK", "How long the sound takes to rise after a key is pressed");
        envelope.addKnob (state, pid::decay, "label_decay", "DECAY", "How long the level takes to fall to the Sustain level");
        envelope.addKnob (state, pid::sustain, "label_sustain", "SUSTAIN", "The level while a key is held");
        envelope.addKnob (state, pid::release, "label_release", "RELEASE", "How long the sound takes to fade after a key is released");
        envelope.addKnob (state, pid::velocity, "label_velocity", "VELOCITY", "How much the level depends on how hard a key is played");

        filter.addKnob (state, pid::cutoff, "label_cutoff", "CUTOFF", "Filter cutoff frequency");
        filter.addKnob (state, pid::resonance, "label_resonance", "RESO", "Resonance: emphasises the cutoff frequency");
        filter.addKnob (state, pid::filterEnv, "label_filter_env", "ENV", "How far the envelope moves the cutoff");

        samplePanel.addKnob (state, pid::pitch, "label_pitch", "PITCH", "Pitch of all samples, in semitones");
        samplePanel.addKnob (state, pid::start, "label_start", "START", "Where in the sample playback starts");
        samplePanel.addKnob (state, pid::gain, "label_gain", "GAIN", "Overall output level");

        chaos.addKnob (state, pid::chaosPitch, "label_chaos_pitch", "PITCH", "Random pitch, up to an octave up or down");
        chaos.addKnob (state, pid::chaosReverse, "label_chaos_reverse", "REVERSE", "The chance of a note playing backwards");
        chaos.addKnob (state, pid::chaosPan, "label_chaos_pan", "PAN", "How far notes scatter across the stereo field");

        stutter.addKnob (state, pid::stutterRate, "label_stutter_rate", "RATE", "Length of the repeated slice");
        stutter.addKnob (state, pid::stutterChance, "label_stutter_chance", "CHANCE", "How often the stutter kicks in");

        dirt.addKnob (state, pid::dirtDrive, "label_dirt_drive", "DRIVE", "Amount of overdrive");
        dirt.addKnob (state, pid::dirtBits, "label_dirt_bits", "BITS", "Bit depth: the lower, the rougher");
        dirt.addKnob (state, pid::dirtRate, "label_dirt_rate", "CRUSH", "Sample rate reduction: the higher, the dirtier the highs");

        echo.addKnob (state, pid::echoTime, "label_echo_time", "TIME", "Echo time, in note lengths");
        echo.addKnob (state, pid::echoFeedback, "label_echo_feedback", "FEEDBACK", "How many repeats you hear");
        echo.addKnob (state, pid::echoMix, "label_echo_mix", "MIX", "Echo level relative to the original sound");

        reverb.addKnob (state, pid::reverbMix, "label_reverb_mix", "MIX", "Reverb level relative to the original sound");
        reverb.addKnob (state, pid::reverbSize, "label_reverb_size", "SIZE", "Size of the room");
        reverb.addKnob (state, pid::reverbDamp, "label_reverb_damp", "DAMP", "How quickly the highs fade in the tail");
    }

    void setupPresets()
    {
        juce::StringArray names;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            names.add (proc.getProgramName (i));

        presetBox.addItemList (names, 1);
        presetBox.setWantsKeyboardFocus (false);
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
        presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedItemIndex()); };
        presetBox.setTooltip ("Ready-made sounds. They leave the Music section as it is");
        addAndMakeVisible (presetBox);

        const auto step = [this] (int delta)
        {
            const auto count = proc.getNumPrograms();
            proc.applyPreset ((proc.getCurrentProgram() + delta + count) % count);
        };

        prevPreset.onClick = [step] { step (-1); };
        nextPreset.onClick = [step] { step (1); };
        addAndMakeVisible (prevPreset);
        addAndMakeVisible (nextPreset);
    }

    RygoProcessor& proc;
    Apvts& state;
    juce::SharedResourcePointer<ArtStore> art;

    MusicPanel music;
    WaveformView waveform;
    KeyMap keys;
    LengthStrip strip;

    Section envelope, filter, samplePanel, chaos, stutter, dirt, echo, reverb;
    SegmentedChoice filterType, mode;
    ArtToggle reverse, trim, normalize, fxMaster;
    juce::ComboBox presetBox;
    ArtButton prevPreset, nextPreset;

    BankPtr shownBank;
    int hoverNote = -1;
    bool isLocked = false;
};

//==============================================================================
RygoEditor::RygoEditor (RygoProcessor& p)
    : juce::AudioProcessorEditor (p), rygoProcessor (p)
{
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);

    // After the panel exists: this also rebuilds the knobs' text boxes in the panel's style.
    setLookAndFeel (&lookAndFeel);

    // Read before setting limits: applying them resizes the window, which would overwrite the saved scale.
    const auto scale = juce::jlimit (0.6, 2.0, p.getLibrary().getUiScale());

    const auto ratio = (double) layout::width / (double) layout::height;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (juce::roundToInt (layout::width * 0.6), juce::roundToInt (layout::height * 0.6),
                     layout::width * 2, layout::height * 2);

    setSize (juce::roundToInt (layout::width * scale), juce::roundToInt (layout::height * scale));

    isConstructed = true;
    content->update();
    startTimerHz (30);
}

RygoEditor::~RygoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RygoEditor::resized()
{
    // The panel is laid out once at 100% and scaled as a whole, so hand-drawn art keeps its positions.
    const auto scale = (float) getWidth() / (float) layout::width;
    content->setTransform (juce::AffineTransform::scale (scale));

    if (isConstructed)
        rygoProcessor.getLibrary().setUiScale ((double) scale);
}

void RygoEditor::timerCallback()
{
    content->update();
}
}
