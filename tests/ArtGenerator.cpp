#include "ArtGenerator.h"
#include "ArtStore.h"
#include "PluginEditor.h"
#include <iostream>

namespace rygo::tests
{
using namespace ui;
using juce::Graphics;
using juce::Image;
using juce::Path;
using juce::Rectangle;

namespace
{
Path wobblyRect (Rectangle<float> r, float amount, juce::uint32 seed)
{
    juce::Random random ((juce::int64) seed);
    Path path;
    const auto wobble = [&] { return (random.nextFloat() - 0.5f) * amount; };

    const auto addEdge = [&] (juce::Point<float> from, juce::Point<float> to)
    {
        const auto length = from.getDistanceFrom (to);
        const auto steps = juce::jmax (2, (int) (length / 90.0f));
        for (int i = 1; i <= steps; ++i)
        {
            const auto p = from + (to - from) * ((float) i / (float) steps);
            path.lineTo (p.x + wobble(), p.y + wobble());
        }
    };

    path.startNewSubPath (r.getX(), r.getY());
    addEdge (r.getTopLeft(), r.getTopRight());
    addEdge (r.getTopRight(), r.getBottomRight());
    addEdge (r.getBottomRight(), r.getBottomLeft());
    addEdge (r.getBottomLeft(), r.getTopLeft());
    path.closeSubPath();
    return path.createPathWithRoundedCorners (10.0f);
}

Path wobblyEllipse (Rectangle<float> r, float amount, juce::uint32 seed)
{
    juce::Random random ((juce::int64) seed);
    const auto p1 = random.nextFloat() * 6.28f;
    const auto p2 = random.nextFloat() * 6.28f;

    Path path;
    for (int i = 0; i <= 72; ++i)
    {
        const auto angle = (float) i / 72.0f * juce::MathConstants<float>::twoPi;
        const auto factor = 1.0f + amount * (std::sin (3.0f * angle + p1) * 0.6f + std::sin (5.0f * angle + p2) * 0.4f);
        const auto x = r.getCentreX() + std::cos (angle) * r.getWidth() * 0.5f * factor;
        const auto y = r.getCentreY() + std::sin (angle) * r.getHeight() * 0.5f * factor;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    return path;
}

void drawPanelLines (Graphics& g, int width)
{
    const auto scale = (float) width / (float) layout::width;

    const auto frame = [&] (Rectangle<int> area, juce::uint32 seed)
    {
        g.setColour (pal::ink.withAlpha (0.8f));
        g.strokePath (wobblyRect ((area.toFloat() * scale).reduced (1.5f), 3.5f, seed), juce::PathStrokeType (2.5f));
    };

    // Lighter "paper" under text that changes while playing, so it stays readable on the tape and glare.
    g.setColour (pal::kraftLight.withAlpha (0.6f));
    g.fillPath (wobblyRect (layout::wave.toFloat() * scale, 3.0f, 3));
    g.fillPath (wobblyRect (layout::music.toFloat() * scale, 3.0f, 4));

    frame (layout::logo.expanded (8, 4), 1);
    frame (layout::music, 4);
    frame (layout::wave, 3);
    frame (layout::envelope, 5);
    frame (layout::filter, 6);
    frame (layout::sample, 7);
    frame (layout::chaos, 8);
    frame (layout::stutter, 9);
    frame (layout::dirt, 10);
    frame (layout::echo, 11);
    frame (layout::reverb, 12);
    frame (layout::keyboard.expanded (4).withTop (layout::strip.getY() - 2), 13);
}

Image render (const ArtItem& item, ArtStore& art)
{
    const auto name = juce::String (item.file);
    const auto width = item.width;
    const auto height = item.height;

    Image image (Image::ARGB, width, height, true);
    Graphics g (image);
    const auto bounds = Rectangle<float> ((float) width, (float) height);

    if (name == "panel_lines")
    {
        drawPanelLines (g, width);
    }
    else if (name == "knob_base")
    {
        const auto area = bounds.reduced (10.0f);
        g.setGradientFill (juce::ColourGradient (pal::kraftLight, area.getX() + area.getWidth() * 0.3f, area.getY() + area.getHeight() * 0.25f,
                                                 pal::kraft, area.getRight(), area.getBottom(), true));
        g.fillEllipse (area);
        g.setColour (pal::ink);
        g.strokePath (wobblyEllipse (area, 0.012f, 21), juce::PathStrokeType (7.0f));
    }
    else if (name == "knob_pointer")
    {
        Path line;
        line.addRoundedRectangle (bounds.getCentreX() - 8.0f, 26.0f, 16.0f, 86.0f, 8.0f);
        g.setColour (pal::ink);
        g.fillPath (line);
    }
    else if (name == "led_on" || name == "led_off")
    {
        const auto area = bounds.reduced (7.0f);
        if (name == "led_on")
        {
            g.setColour (pal::hot);
            g.fillEllipse (area);
        }
        g.setColour (pal::ink);
        g.strokePath (wobblyEllipse (area, 0.02f, 22), juce::PathStrokeType (5.0f));
    }
    else if (name == "mark_selected")
    {
        g.setColour (pal::ink);
        g.strokePath (wobblyEllipse (bounds.reduced (12.0f, 10.0f), 0.018f, 23), juce::PathStrokeType (6.0f));
        g.strokePath (wobblyEllipse (bounds.reduced (9.0f, 12.0f), 0.022f, 24), juce::PathStrokeType (3.0f));
    }
    else if (name == "lock_open" || name == "lock_closed")
    {
        const auto s = (float) width;
        const auto body = Rectangle<float> (s * 0.2f, s * 0.46f, s * 0.6f, s * 0.42f);

        // The shackle: a closed lock's reaches into the body, an open one's lifts and swings aside.
        Path shackle;
        const auto arcWidth = s * 0.36f;
        const auto lift = name == "lock_open" ? s * 0.09f : 0.0f;
        const auto left = name == "lock_open" ? s * 0.4f : s * 0.32f;
        shackle.addCentredArc (left + arcWidth * 0.5f, s * 0.3f - lift, arcWidth * 0.5f, arcWidth * 0.5f, 0.0f,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        shackle.lineTo (left + arcWidth, s * 0.46f - lift);
        shackle.startNewSubPath (left, s * 0.3f - lift);
        shackle.lineTo (left, name == "lock_open" ? s * 0.3f : s * 0.46f);
        g.setColour (pal::ink);
        g.strokePath (shackle, juce::PathStrokeType (s * 0.055f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (name == "lock_closed" ? pal::ink : pal::kraftLight);
        g.fillRoundedRectangle (body, s * 0.04f);
        g.setColour (pal::ink);
        g.drawRoundedRectangle (body, s * 0.04f, s * 0.025f);

        const auto keyhole = Rectangle<float> (s * 0.1f, s * 0.1f).withCentre (body.getCentre());
        g.setColour (name == "lock_closed" ? pal::kraftLight : pal::ink);
        g.fillEllipse (keyhole);
    }
    else if (name == "arrow_prev" || name == "arrow_next")
    {
        Path arrow;
        const auto s = (float) width;
        if (name == "arrow_prev")
            arrow.addTriangle (s * 0.68f, s * 0.2f, s * 0.68f, s * 0.8f, s * 0.26f, s * 0.5f);
        else
            arrow.addTriangle (s * 0.32f, s * 0.2f, s * 0.32f, s * 0.8f, s * 0.74f, s * 0.5f);
        g.setColour (pal::ink);
        g.fillPath (arrow);
    }
    else
    {
        drawPlaceholder (g, bounds, item.letter, art.font ((float) height * 0.56f, true));
    }

    return image;
}

Image snapshotEditor (bool locked = false)
{
    RygoProcessor processor;
    processor.setNonRealtime (true);
    processor.prepareToPlay (48000.0, 512);

    // The panel at its most useful: switched on, so everything on it can be seen.
    processor.getState().getParameter (pid::musicOn)->setValueNotifyingHost (1.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 127), 0);
    processor.processBlock (buffer, midi);

    if (locked)
        processor.getState().getParameter (pid::musicLock)->setValueNotifyingHost (1.0f);

    processor.getLibrary().setUiScale (1.0);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    return editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
}
}

bool writePng (const Image& image, const juce::File& file)
{
    file.deleteFile();
    file.getParentDirectory().createDirectory();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat format;
    return stream.openedOk() && format.writeImageToStream (image, stream);
}

int makeArt (const juce::File& artDir, const juce::File& guideDir)
{
    artDir.createDirectory();
    guideDir.createDirectory();

    // Files that already exist are left alone: they may be real drawings.
    int created = 0, kept = 0, total = 0;
    juce::String list;

    {
        juce::SharedResourcePointer<ArtStore> art;

        list << ru ("# Что нарисовать для «Рыгостанции»\n\n")
             << ru ("Все файлы лежат в папке `art/`. Замени заглушку своим рисунком с тем же именем, запусти `scripts/update_art.sh` и заново открой окно плагина: пересобирать ничего не нужно.\n\n")
             << ru ("- Формат: PNG с прозрачным фоном.\n")
             << ru ("- Имена файлов менять нельзя. Текст надписей менять можно, плагин видит только имя файла. В списке он на английском, стилизованные элементы можешь рисовать на кириллице.\n")
             << ru ("- Размер холста рекомендованный. Другие пропорции тоже подойдут: плагин впишет рисунок в отведённое место, не растягивая его.\n")
             << ru ("- Оставляй небольшие поля по краям холста.\n")
             << ru ("- Где что находится, показывает `art_guide/layout_template.png`. Удобно рисовать поверх него слоем.\n\n");

        for (const auto& group : getArtManifest())
        {
            list << "## " << group.title << " (" << (int) group.items.size() << ")\n\n" << group.advice << "\n\n";

            for (const auto& item : group.items)
            {
                ++total;
                const auto file = artDir.getChildFile (juce::String (item.file) + ".png");

                list << "- [ ] `" << item.file << ".png`: " << item.letter
                     << " (" << item.width << "x" << item.height << ")\n";

                if (file.existsAsFile())
                {
                    ++kept;
                    continue;
                }

                if (! writePng (render (item, *art), file))
                {
                    std::cerr << "Could not write " << file.getFullPathName() << std::endl;
                    return 1;
                }
                ++created;
            }
            list << "\n";
        }

        list << ru ("## Шрифт (по желанию)\n\n")
             << ru ("Числа на крутилках, названия нот и пресетов меняются прямо в работе, поэтому заранее их не нарисовать. Они набираются шрифтом. ")
             << ru ("Если хочешь рукописный, положи его как `art/font.ttf`: нужны латиница, цифры и знаки `. , - + / # %`. Без этого файла используется системный шрифт.\n\n")
             << ru ("Клавиатуру и волну рисует сам плагин, рисовать их не нужно.\n");
    }

    if (! guideDir.getChildFile ("ART_LIST.md").replaceWithText (list))
        return 1;

    if (! writePng (snapshotEditor(), guideDir.getChildFile ("layout_template.png")))
        return 1;

    std::cout << "art items: " << total << ", created " << created << ", kept " << kept << std::endl;
    return 0;
}

int snapshot (const juce::File& output, bool locked)
{
    return writePng (snapshotEditor (locked), output) ? 0 : 1;
}
}
