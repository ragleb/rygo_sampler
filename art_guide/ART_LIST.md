# Что нарисовать для «Рыгостанции»

Все файлы лежат в папке `art/`. Замени заглушку своим рисунком с тем же именем, запусти `scripts/update_art.sh` и заново открой окно плагина: пересобирать ничего не нужно.

- Формат: PNG с прозрачным фоном.
- Имена файлов менять нельзя. Текст надписей менять можно, плагин видит только имя файла. В списке он на английском, стилизованные элементы можешь рисовать на кириллице.
- Размер холста рекомендованный. Другие пропорции тоже подойдут: плагин впишет рисунок в отведённое место, не растягивая его.
- Оставляй небольшие поля по краям холста.
- Где что находится, показывает `art_guide/layout_template.png`. Удобно рисовать поверх него слоем.

## Логотипы (2)

Самое заметное в плагине: рисуй крупно и свободно.

- [ ] `logo_rygostancija.png`: RYGOSTANCIJA (1600x336)
- [ ] `logo_zhri.png`: ZHRI Records (680x256)

## Фон (1)

Картон уже стоит: это твоя текстура, файл art/background.jpg. Рисовать нужно только второй слой: линии маркером поверх картона, с прозрачным фоном. Сейчас в нём простые рамки-заглушки. Под панелью MUSIC и под волной оставь светлую подложку: там текст меняется во время работы, а на блике от скотча он читается хуже.

- [ ] `panel_lines.png`: Рамки, узоры и разделители маркером (2360x1520)

## Крутилка (2)

Один рисунок на все крутилки. Центр вращения совпадает с центром холста, рисуй ровно по центру.

- [ ] `knob_base.png`: Корпус крутилки (неподвижный) (256x256)
- [ ] `knob_pointer.png`: Указатель, смотрит строго вверх (вращает плагин) (256x256)

## Элементы управления (5)

Индикаторы и пометки выбранного. mark_selected растягивается под размер подписи.

- [ ] `led_on.png`: Индикатор: включено (64x64)
- [ ] `led_off.png`: Индикатор: выключено (64x64)
- [ ] `mark_selected.png`: Обводка или подчёркивание выбранного варианта (400x120)
- [ ] `arrow_prev.png`: Стрелка влево (96x96)
- [ ] `arrow_next.png`: Стрелка вправо (96x96)

## Заголовки блоков (11)

Подписи крупных блоков.

- [ ] `title_presets.png`: PRESET (400x80)
- [ ] `title_fx.png`: EFFECTS (400x80)
- [ ] `title_music.png`: MUSIC (400x80)
- [ ] `title_envelope.png`: ENVELOPE (400x80)
- [ ] `title_filter.png`: FILTER (400x80)
- [ ] `title_sample.png`: SAMPLE (400x80)
- [ ] `title_chaos.png`: CHAOS (400x80)
- [ ] `title_stutter.png`: STUTTER (400x80)
- [ ] `title_dirt.png`: DIRT (400x80)
- [ ] `title_echo.png`: ECHO (400x80)
- [ ] `title_reverb.png`: REVERB (400x80)

## Подписи крутилок (25)

Самые мелкие надписи: на экране они высотой примерно 16 точек, поэтому рисуй разборчиво и жирно.

- [ ] `label_attack.png`: ATTACK (248x64)
- [ ] `label_decay.png`: DECAY (248x64)
- [ ] `label_sustain.png`: SUSTAIN (248x64)
- [ ] `label_release.png`: RELEASE (248x64)
- [ ] `label_velocity.png`: VELOCITY (248x64)
- [ ] `label_cutoff.png`: CUTOFF (248x64)
- [ ] `label_resonance.png`: RESO (248x64)
- [ ] `label_filter_env.png`: ENV (248x64)
- [ ] `label_pitch.png`: PITCH (248x64)
- [ ] `label_start.png`: START (248x64)
- [ ] `label_gain.png`: GAIN (248x64)
- [ ] `label_chaos_pitch.png`: PITCH (248x64)
- [ ] `label_chaos_reverse.png`: REVERSE (248x64)
- [ ] `label_chaos_pan.png`: PAN (248x64)
- [ ] `label_stutter_rate.png`: RATE (248x64)
- [ ] `label_stutter_chance.png`: CHANCE (248x64)
- [ ] `label_dirt_drive.png`: DRIVE (248x64)
- [ ] `label_dirt_bits.png`: BITS (248x64)
- [ ] `label_dirt_rate.png`: CRUSH (248x64)
- [ ] `label_echo_time.png`: TIME (248x64)
- [ ] `label_echo_feedback.png`: FEEDBACK (248x64)
- [ ] `label_echo_mix.png`: MIX (248x64)
- [ ] `label_reverb_mix.png`: MIX (248x64)
- [ ] `label_reverb_size.png`: SIZE (248x64)
- [ ] `label_reverb_damp.png`: DAMP (248x64)

## Режимы и кнопки (11)

Варианты, которые видны сразу.

- [ ] `opt_oneshot.png`: ONE SHOT (320x72)
- [ ] `opt_gate.png`: GATE (220x72)
- [ ] `opt_loop.png`: LOOP (220x72)
- [ ] `opt_lp.png`: LP (160x72)
- [ ] `opt_hp.png`: HP (160x72)
- [ ] `opt_bp.png`: BP (160x72)
- [ ] `opt_reverse.png`: REVERSE (400x72)
- [ ] `opt_trim.png`: TRIM SILENCE (400x72)
- [ ] `opt_normalize.png`: NORMALIZE (400x72)
- [ ] `hint_long.png`: LONG (400x72)
- [ ] `hint_short.png`: SHORT (400x72)

## Music (6)

Панель слева. Замочек рисуй в обоих вариантах на одном месте холста, чтобы при переключении он не прыгал.

- [ ] `lock_open.png`: Замочек открыт: выбор семпла (320x320)
- [ ] `lock_closed.png`: Замочек закрыт: семпл на всех клавишах (320x320)
- [ ] `label_lock.png`: SAMPLE LOCK (640x80)
- [ ] `label_sample.png`: SAMPLE (640x68)
- [ ] `label_root.png`: ROOT (640x68)
- [ ] `opt_keep_length.png`: KEEP LENGTH (500x80)

## Шрифт (по желанию)

Числа на крутилках, названия нот и пресетов меняются прямо в работе, поэтому заранее их не нарисовать. Они набираются шрифтом. Если хочешь рукописный, положи его как `art/font.ttf`: нужны латиница, цифры и знаки `. , - + / # %`. Без этого файла используется системный шрифт.

Клавиатуру и волну рисует сам плагин, рисовать их не нужно.
