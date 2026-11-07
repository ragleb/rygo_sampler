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
        { ru ("Логотипы"), ru ("Самое заметное в плагине: рисуй крупно и свободно."),
          {
              { "logo_rygostancija", "RYGOSTANCIJA", 1600, 336 },
              { "logo_zhri",         "ZHRI Records",  680, 256 },
          } },

        { ru ("Фон"), ru ("Картон уже стоит: это твоя текстура, файл art/background.jpg. Рисовать нужно только второй слой: линии маркером поверх картона, с прозрачным фоном. Сейчас в нём простые рамки-заглушки. Под панелью MUSIC и под волной оставь светлую подложку: там текст меняется во время работы, а на блике от скотча он читается хуже."),
          {
              { "panel_lines", ru ("Рамки, узоры и разделители маркером"), 2360, 1520 },
          } },

        { ru ("Крутилка"), ru ("Один рисунок на все крутилки. Центр вращения совпадает с центром холста, рисуй ровно по центру."),
          {
              { "knob_base",    ru ("Корпус крутилки (неподвижный)"),                 256, 256 },
              { "knob_pointer", ru ("Указатель, смотрит строго вверх (вращает плагин)"), 256, 256 },
          } },

        { ru ("Элементы управления"), ru ("Индикаторы и пометки выбранного. mark_selected растягивается под размер подписи."),
          {
              { "led_on",        ru ("Индикатор: включено"),  64, 64 },
              { "led_off",       ru ("Индикатор: выключено"), 64, 64 },
              { "mark_selected", ru ("Обводка или подчёркивание выбранного варианта"), 400, 120 },
              { "arrow_prev",    ru ("Стрелка влево"),  96, 96 },
              { "arrow_next",    ru ("Стрелка вправо"), 96, 96 },
          } },

        { ru ("Заголовки блоков"), ru ("Подписи крупных блоков."),
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

        { ru ("Подписи крутилок"), ru ("Самые мелкие надписи: на экране они высотой примерно 16 точек, поэтому рисуй разборчиво и жирно."),
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

        { ru ("Режимы и кнопки"), ru ("Варианты, которые видны сразу."),
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

        { "Music", ru ("Панель слева. Замочек рисуй в обоих вариантах на одном месте холста, чтобы при переключении он не прыгал."),
          {
              { "lock_open",       ru ("Замочек открыт: выбор семпла"),   320, 320 },
              { "lock_closed",     ru ("Замочек закрыт: семпл на всех клавишах"), 320, 320 },
              { "label_lock",      "SAMPLE LOCK",  640, 80 },
              { "label_sample",    "SAMPLE",       640, 68 },
              { "label_root",      "ROOT",         640, 68 },
              { "opt_keep_length", "KEEP LENGTH",  500, 80 },
          } },
    };

    return groups;
}
}
