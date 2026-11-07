# What to draw for Rygostancija

All the files live in the `art/` folder. Replace a placeholder with your drawing under the same name, run `scripts/update_art.sh` and reopen the plugin window: no rebuild is needed.

- Format: PNG with a transparent background.
- Don't change the file names. You can change the wording of a label, since the plugin only sees the file name. The list gives it in English; stylized elements can be drawn in Cyrillic.
- The canvas size is a recommendation. Other proportions work too: the plugin fits the drawing into its place without stretching it.
- Leave small margins at the edges of the canvas.
- `art_guide/layout_template.png` shows where everything goes. It's handy to draw over it on a layer.

## Logos (2)

The most visible part of the plugin: draw them large and freely.

- [ ] `logo_rygostancija.png`: RYGOSTANCIJA (1600x336)
- [ ] `logo_zhri.png`: ZHRI Records (680x256)

## Background (1)

The cardboard is already in place: it's your texture, the file art/background.jpg. You only need to draw the second layer: marker lines over the cardboard, with a transparent background. The placeholder version has simple frames. Leave a lighter backing under the MUSIC panel and the waveform: the text there changes while the plugin runs, and it's harder to read over the glare from the tape.

- [ ] `panel_lines.png`: Marker frames, patterns and dividers (2360x1520)

## Knob (2)

One drawing for all the knobs. The center of rotation is the center of the canvas, so draw exactly in the center.

- [ ] `knob_base.png`: Knob body (stays still) (256x256)
- [ ] `knob_pointer.png`: Pointer, facing straight up (the plugin rotates it) (256x256)

## Controls (5)

Indicators and selection marks. mark_selected stretches to the size of its label.

- [ ] `led_on.png`: Indicator: on (64x64)
- [ ] `led_off.png`: Indicator: off (64x64)
- [ ] `mark_selected.png`: Outline or underline for the selected option (400x120)
- [ ] `arrow_prev.png`: Left arrow (96x96)
- [ ] `arrow_next.png`: Right arrow (96x96)

## Section titles (11)

The labels of the large sections.

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

## Knob labels (25)

The smallest lettering: about 16 points tall on screen, so draw it legibly and boldly.

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

## Modes and buttons (11)

Options that are visible all the time.

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

The panel on the left. Draw both versions of the lock in the same place on the canvas, so it doesn't jump when it switches.

- [ ] `lock_open.png`: Lock open: choosing a sample (320x320)
- [ ] `lock_closed.png`: Lock closed: the sample is on every key (320x320)
- [ ] `label_lock.png`: SAMPLE LOCK (640x80)
- [ ] `label_sample.png`: SAMPLE (640x68)
- [ ] `label_root.png`: ROOT (640x68)
- [ ] `opt_keep_length.png`: KEEP LENGTH (500x80)

## Font (optional)

The numbers on the knobs and the names of notes and presets change while the plugin runs, so they can't be drawn in advance. They are set in a font. If you want a handwritten one, put it at `art/font.ttf`: it needs Latin letters, digits and the characters `. , - + / # %`. Without this file, the system font is used.

The plugin draws the keyboard and the waveform itself, so there's no need to draw them.
