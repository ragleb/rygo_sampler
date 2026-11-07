#!/bin/bash
# Копирует собранный плагин в системные папки и просит macOS заново его найти.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARTEFACTS="$ROOT/build/Rygostancija_artefacts/Release"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/Rygostancija.component" "$VST3_DIR/Rygostancija.vst3"
cp -R "$ARTEFACTS/AU/Rygostancija.component" "$AU_DIR/"
cp -R "$ARTEFACTS/VST3/Rygostancija.vst3" "$VST3_DIR/"

# Заставляет систему перечитать список Audio Unit, иначе Logic может не увидеть новую версию.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Установлено:"
echo "  $AU_DIR/Rygostancija.component"
echo "  $VST3_DIR/Rygostancija.vst3"
