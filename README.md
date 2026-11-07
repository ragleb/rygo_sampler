# Рыгостанция (Rygostancija)

Плагин-сэмплер ЖРИ Records (ZHRI Records) для Logic Pro и REAPER: форматы AU и VST3 на macOS, Apple Silicon и Intel. VST3 для 64-битной Windows собирается через GitHub Actions.

## Где что лежит

| Папка | Что внутри |
|---|---|
| `factory_samples/` | Семплы плагина. Новый файл здесь сам встанет на клавиатуру по своей длине после пересборки |
| `art/` | Весь внешний вид: твои PNG заменяют заглушки |
| `art_guide/` | `ART_LIST.md` со списком того, что нарисовать, и `layout_template.png` с расположением |
| `source/` | Код плагина |
| `tests/` | Проверки движка и загрузки плагина |
| `scripts/` | Сборка, установка и упаковка |

## Заменить рисунки

Положи файлы с теми же именами в `art/` и запусти:

```bash
scripts/update_art.sh
```

Затем заново открой окно плагина. Пересобирать ничего не нужно.

## Собрать и установить

Нужны Xcode и CMake. Если JUCE нет в `third_party/JUCE`, CMake скачает его сам.

```bash
scripts/build.sh
```

## Собрать архив для раздачи

```bash
scripts/package_mac.sh
```

Архив появится в `dist/`. Сейчас плагин подписан без сертификата Developer ID, поэтому при установке macOS покажет предупреждение. Как его обойти, написано в архиве.

## Собрать для Windows

Сборку запускает `.github/workflows/build.yml`: после каждого пуша в `main` или кнопкой Run workflow на GitHub. На Mac версию для Windows собрать нельзя. Готовый `Rygostancija-<версия>-windows.zip` лежит в артефактах запуска 30 дней. Внутри плагин, `install.bat` и `README.txt`.

## Проверки

```bash
cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DRYGO_UNIVERSAL=OFF
```

```bash
cmake --build build-dev --target RygoTests
```

```bash
RYGO_FACTORY_SAMPLES="$PWD/factory_samples" RYGO_ART_DIR="$PWD/art" build-dev/RygoTests_artefacts/Release/RygoTests
```

Загрузка установленного плагина через настоящие обёртки (нужна цель `RygoHostCheck`) и через системный API Apple:

```bash
build/RygoHostCheck_artefacts/Release/RygoHostCheck ~/Library/Audio/Plug-Ins/Components/Rygostancija.component
```

```bash
build/RygoHostCheck_artefacts/Release/RygoHostCheck ~/Library/Audio/Plug-Ins/VST3/Rygostancija.vst3
```

```bash
swift tests/au_render_check.swift
```

## Лицензия JUCE

Плагин собран на JUCE 9, который распространяется по AGPLv3 или по коммерческой лицензии. Бесплатный тариф Starter действует, пока доход или финансирование меньше $20 000 за последние 12 месяцев. Для компании считается весь её доход из любых источников, а для частного лица доход от связанной с JUCE деятельности, включая донаты. При превышении порога нужна платная лицензия. Условия: https://juce.com/legal/juce-9-licence/
