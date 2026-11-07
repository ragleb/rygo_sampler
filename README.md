# Rygostancija

A free sampler plugin by ZHRI Records (ЖРИ Records) for Logic Pro, REAPER and other hosts: AU and VST3 on macOS (Apple Silicon and Intel), VST3 on 64-bit Windows.

## Download

Get the latest version from [Releases](https://github.com/ragleb/rygo_sampler/releases/latest). Each archive contains an installer and a read-me file.

The plugin has no paid code signing certificate yet, so macOS and Windows show a warning during installation. The read-me file explains how to get past it.

## Where things live

| Folder | What's inside |
|---|---|
| `factory_samples/` | The plugin's samples. After a rebuild, a new file here takes its place on the keyboard according to its length |
| `art/` | The plugin's entire look. PNG files with the same names replace what's here |
| `art_guide/` | `ART_LIST.md` lists everything to draw, and `layout_template.png` shows where it goes |
| `source/` | The plugin's code |
| `tests/` | Checks of the engine and of loading the plugin |
| `scripts/` | Building, installing and packaging |

## Replace the art on a Mac

Put files with the same names into `art/` and run:

```bash
scripts/update_art.sh
```

Then reopen the plugin window. No rebuild is needed.

## Build and install on a Mac

You need Xcode and CMake. If JUCE isn't in `third_party/JUCE`, CMake downloads it.

```bash
scripts/build.sh
```

## Package for Mac

```bash
scripts/package_mac.sh
```

The archive appears in `dist/`. The plugin is signed without a Developer ID certificate, so macOS shows a warning during installation. The archive explains how to get past it.

## Build for Windows

`.github/workflows/build.yml` runs the build after every push to `main`, or when you click Run workflow on GitHub. The Windows version can't be built on a Mac. The finished `Rygostancija-<version>-windows.zip` stays in the run's artifacts for 30 days. It contains the plugin, `install.bat` and `README.txt`.

## Tests

```bash
cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DRYGO_UNIVERSAL=OFF
```

```bash
cmake --build build-dev --target RygoTests
```

```bash
RYGO_FACTORY_SAMPLES="$PWD/factory_samples" RYGO_ART_DIR="$PWD/art" build-dev/RygoTests_artefacts/Release/RygoTests
```

Load the installed plugin through the real wrappers (this needs the `RygoHostCheck` target) and through Apple's system API:

```bash
build/RygoHostCheck_artefacts/Release/RygoHostCheck ~/Library/Audio/Plug-Ins/Components/Rygostancija.component
```

```bash
build/RygoHostCheck_artefacts/Release/RygoHostCheck ~/Library/Audio/Plug-Ins/VST3/Rygostancija.vst3
```

```bash
swift tests/au_render_check.swift
```

## JUCE licence

The plugin is built on JUCE 9, which is available under AGPLv3 or a commercial licence. The free Starter tier applies while revenue or funding stays under $20,000 over the last 12 months. For a company, that counts all its income from any source. For an individual, it counts income from JUCE-related activity, including donations. Above that limit, a paid licence is required. Terms: https://juce.com/legal/juce-9-licence/
