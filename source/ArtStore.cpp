#include "ArtStore.h"
#include <cstdlib>

namespace rygo::ui
{
ArtStore::ArtStore()
{
    if (const auto* overrideDir = std::getenv ("RYGO_ART_DIR"))
    {
        folder = juce::File (juce::String::fromUTF8 (overrideDir));
    }
    else
    {
        auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        for (int depth = 0; depth < 3 && ! folder.isDirectory(); ++depth)
        {
            folder = dir.getChildFile ("Resources").getChildFile ("Art");
            dir = dir.getParentDirectory();
        }
    }

    const auto fontFile = folder.getChildFile ("font.ttf");
    if (fontFile.existsAsFile())
    {
        juce::MemoryBlock data;
        if (fontFile.loadFileAsData (data))
            typeface = juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
    }
}

juce::Image ArtStore::image (const juce::String& name)
{
    const auto found = cache.find (name);
    if (found != cache.end())
        return found->second;

    auto file = folder.getChildFile (name + ".png");
    if (! file.existsAsFile())
        file = folder.getChildFile (name + ".jpg");   // photographic textures are far smaller as JPEG

    auto loaded = juce::ImageFileFormat::loadFrom (file);
    cache[name] = loaded;
    return loaded;
}

juce::Font ArtStore::font (float size, bool bold) const
{
    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withHeight (size));

    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

void ArtStore::drawArt (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area,
                        const juce::String& placeholderText, juce::RectanglePlacement placement)
{
    const auto picture = image (name);

    if (picture.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (picture, area, placement);
    }
    else
    {
        drawPlaceholder (g, area, placeholderText, font (juce::jmin (13.0f, area.getHeight() * 0.8f)));
    }
}

void drawPlaceholder (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const juce::Font& font)
{
    juce::Path outline;
    outline.addRoundedRectangle (area.reduced (1.0f), juce::jmin (6.0f, area.getHeight() * 0.25f));

    const float dashes[] { 4.0f, 3.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);

    g.setColour (pal::ink.withAlpha (0.3f));
    g.fillPath (dashed);

    g.setColour (pal::ink);
    g.setFont (font);
    g.drawFittedText (text, area.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.5f);
}

const std::vector<ArtGroup>& getArtManifest()
{
    static const std::vector<ArtGroup> groups
    {
        { "Logos", "The most visible part of the plugin: draw them large and freely.",
          {
              { "logo_rygostancija", "RYGOSTANCIJA", 1600, 336 },
              { "logo_zhri",         "ZHRI Records",  680, 256 },
          } },

        { "Background", "The cardboard is already in place: it's your texture, the file art/background.jpg. You only need to draw the second layer: marker lines over the cardboard, with a transparent background. The placeholder version has simple frames. Leave a lighter backing under the MUSIC panel and the waveform: the text there changes while the plugin runs, and it's harder to read over the glare from the tape.",
          {
              { "panel_lines", "Marker frames, patterns and dividers", 2360, 1520 },
          } },

        { "Knob", "One drawing for all the knobs. The center of rotation is the center of the canvas, so draw exactly in the center.",
          {
              { "knob_base",    "Knob body (stays still)",                 256, 256 },
              { "knob_pointer", "Pointer, facing straight up (the plugin rotates it)", 256, 256 },
          } },

        { "Controls", "Indicators and selection marks. mark_selected stretches to the size of its label.",
          {
              { "led_on",        "Indicator: on",  64, 64 },
              { "led_off",       "Indicator: off", 64, 64 },
              { "mark_selected", "Outline or underline for the selected option", 400, 120 },
              { "arrow_prev",    "Left arrow",  96, 96 },
              { "arrow_next",    "Right arrow", 96, 96 },
          } },

        { "Section titles", "The labels of the large sections.",
          {
              { "title_presets",  "PRESET",   400, 80 },
              { "title_fx",       "EFFECTS",  400, 80 },
              { "title_music",    "MUSIC",    400, 80 },
              { "title_envelope", "ENVELOPE", 400, 80 },
              { "title_filter",   "FILTER",   400, 80 },
              { "title_sample",   "SAMPLE",   400, 80 },
              { "title_chaos",    "CHAOS",    400, 80 },
              { "title_stutter",  "STUTTER",  400, 80 },
              { "title_dirt",     "DIRT",     400, 80 },
              { "title_echo",     "ECHO",     400, 80 },
              { "title_reverb",   "REVERB",   400, 80 },
          } },

        { "Knob labels", "The smallest lettering: about 16 points tall on screen, so draw it legibly and boldly.",
          {
              { "label_attack",       "ATTACK",   248, 64 },
              { "label_decay",        "DECAY",    248, 64 },
              { "label_sustain",      "SUSTAIN",  248, 64 },
              { "label_release",      "RELEASE",  248, 64 },
              { "label_velocity",     "VELOCITY", 248, 64 },
              { "label_cutoff",       "CUTOFF",   248, 64 },
              { "label_resonance",    "RESO",     248, 64 },
              { "label_filter_env",   "ENV",      248, 64 },
              { "label_pitch",        "PITCH",    248, 64 },
              { "label_start",        "START",    248, 64 },
              { "label_gain",         "GAIN",     248, 64 },
              { "label_chaos_pitch",    "PITCH",    248, 64 },
              { "label_chaos_reverse",  "REVERSE",  248, 64 },
              { "label_chaos_pan",      "PAN",      248, 64 },
              { "label_stutter_rate",   "RATE",     248, 64 },
              { "label_stutter_chance", "CHANCE",   248, 64 },
              { "label_dirt_drive",     "DRIVE",    248, 64 },
              { "label_dirt_bits",      "BITS",     248, 64 },
              { "label_dirt_rate",      "CRUSH",    248, 64 },
              { "label_echo_time",      "TIME",     248, 64 },
              { "label_echo_feedback",  "FEEDBACK", 248, 64 },
              { "label_echo_mix",       "MIX",      248, 64 },
              { "label_reverb_mix",     "MIX",      248, 64 },
              { "label_reverb_size",    "SIZE",     248, 64 },
              { "label_reverb_damp",    "DAMP",     248, 64 },
          } },

        { "Modes and buttons", "Options that are visible all the time.",
          {
              { "opt_oneshot",     "ONE SHOT",     320, 72 },
              { "opt_gate",        "GATE",         220, 72 },
              { "opt_loop",        "LOOP",         220, 72 },
              { "opt_lp",          "LP",           160, 72 },
              { "opt_hp",          "HP",           160, 72 },
              { "opt_bp",          "BP",           160, 72 },
              { "opt_reverse",     "REVERSE",      400, 72 },
              { "opt_trim",        "TRIM SILENCE", 400, 72 },
              { "opt_normalize",   "NORMALIZE",    400, 72 },
              { "hint_long",       "LONG",         400, 72 },
              { "hint_short",      "SHORT",        400, 72 },
          } },

        { "Music", "The panel on the left. Draw both versions of the lock in the same place on the canvas, so it doesn't jump when it switches.",
          {
              { "lock_open",       "Lock open: choosing a sample",   320, 320 },
              { "lock_closed",     "Lock closed: the sample is on every key", 320, 320 },
              { "label_lock",      "SAMPLE LOCK",  640, 80 },
              { "label_sample",    "SAMPLE",       640, 68 },
              { "label_root",      "ROOT",         640, 68 },
              { "opt_keep_length", "KEEP LENGTH",  500, 80 },
          } },
    };

    return groups;
}
}
