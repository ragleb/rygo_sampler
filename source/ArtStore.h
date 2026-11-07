#pragma once

#include "SampleLibrary.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

namespace rygo::ui
{
namespace pal
{
    inline const juce::Colour kraft      { 0xffa0896e };   // the average tone of the cardboard texture
    inline const juce::Colour kraftDark  { 0xff7f6a50 };
    inline const juce::Colour kraftLight { 0xffdccdb0 };
    inline const juce::Colour ink        { 0xff131a30 };
    inline const juce::Colour hot        { 0xffb8351f };
}

/** All hand-drawn art lives as PNG files in <bundle>/Contents/Resources/Art. Replacing a file
    and reopening the plugin window is all it takes: nothing needs to be recompiled.
*/
class ArtStore
{
public:
    ArtStore();

    /** Invalid if the file doesn't exist; drawArt() then falls back to a placeholder. */
    juce::Image image (const juce::String& name);

    /** Text that can't be drawn in advance (values, folder names) uses art/font.ttf, if present. */
    juce::Font font (float size, bool bold = false) const;

    /** Draws the image fitted into the area, keeping its proportions. */
    void drawArt (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area,
                  const juce::String& placeholderText,
                  juce::RectanglePlacement placement = juce::RectanglePlacement::centred);

private:
    juce::File folder;
    std::map<juce::String, juce::Image> cache;
    juce::Typeface::Ptr typeface;

    JUCE_DECLARE_NON_COPYABLE (ArtStore)
};

void drawPlaceholder (juce::Graphics&, juce::Rectangle<float> area, const juce::String& text, const juce::Font& font);

//==============================================================================
/** The single list of everything to be drawn. The placeholder files, the drawing list
    and the plugin all read from here, so they can't get out of step.
*/
struct ArtItem
{
    const char* file;      // without extension
    juce::String letter;   // what to write or draw
    int width, height;     // recommended canvas, in pixels
};

struct ArtGroup
{
    juce::String title, advice;
    std::vector<ArtItem> items;
};

const std::vector<ArtGroup>& getArtManifest();
}
