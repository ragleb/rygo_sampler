#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rygo::tests
{
/** Writes a placeholder PNG for every art item that doesn't exist yet, the drawing list and the layout template. */
int makeArt (const juce::File& artDir, const juce::File& guideDir);

/** Renders the plugin's window to a PNG at 2x. */
int snapshot (const juce::File& output, bool locked = false);

bool writePng (const juce::Image& image, const juce::File& file);
}
