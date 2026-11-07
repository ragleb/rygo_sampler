#!/bin/bash
# Кладёт рисунки из папки art/ в уже установленный плагин. Пересобирать ничего не нужно:
# запусти этот файл и заново открой окно плагина. Если Logic держит старые картинки, перезапусти его.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
updated=0

for bundle in "$HOME/Library/Audio/Plug-Ins/Components/Rygostancija.component" \
              "$HOME/Library/Audio/Plug-Ins/VST3/Rygostancija.vst3"; do
    if [ -d "$bundle" ]; then
        rm -rf "$bundle/Contents/Resources/Art"
        cp -R "$ROOT/art" "$bundle/Contents/Resources/Art"
        # Метки Finder и пометки о скачивании на рисунках не дают codesign подписать плагин.
        xattr -cr "$bundle/Contents/Resources/Art"
        codesign --force --deep --sign - "$bundle"
        echo "Обновлено: $bundle"
        updated=$((updated + 1))
    fi
done

if [ "$updated" -eq 0 ]; then
    echo "Плагин не установлен. Сначала запусти scripts/build.sh"
    exit 1
fi
