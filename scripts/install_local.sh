#!/bin/bash
# Copies the built plugin into the system folders and asks macOS to find it again.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARTEFACTS="$ROOT/build/Rygostancija_artefacts/Release"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/Rygostancija.component" "$VST3_DIR/Rygostancija.vst3"
cp -R "$ARTEFACTS/AU/Rygostancija.component" "$AU_DIR/"
cp -R "$ARTEFACTS/VST3/Rygostancija.vst3" "$VST3_DIR/"

# Makes macOS reread the list of Audio Units, otherwise Logic may not see the new version.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed:"
echo "  $AU_DIR/Rygostancija.component"
echo "  $VST3_DIR/Rygostancija.vst3"
