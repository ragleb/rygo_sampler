#!/bin/bash
# Rygostancija installer for macOS. Double-click to run.

HERE="$(cd "$(dirname "$0")" && pwd)"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

echo "Installing Rygostancija (ZHRI Records)..."
mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/Rygostancija.component" "$VST3_DIR/Rygostancija.vst3"
cp -R "$HERE/Rygostancija.component" "$AU_DIR/"
cp -R "$HERE/Rygostancija.vst3" "$VST3_DIR/"

# The plugin isn't signed with an Apple certificate: without this, macOS won't load a downloaded copy.
xattr -dr com.apple.quarantine "$AU_DIR/Rygostancija.component"
xattr -dr com.apple.quarantine "$VST3_DIR/Rygostancija.vst3"

# So that Logic sees the new version right away.
killall -9 AudioComponentRegistrar 2>/dev/null

echo
echo "Done. Restart Logic or REAPER and look for the plugin under ZHRI Records."
echo
read -r -p "Press Enter to close this window..."
