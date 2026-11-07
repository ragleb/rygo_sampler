#!/bin/bash
# Puts the drawings from the art/ folder into the installed plugin. No rebuild is needed:
# run this file and reopen the plugin window. If Logic holds on to the old pictures, restart it.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
updated=0

for bundle in "$HOME/Library/Audio/Plug-Ins/Components/Rygostancija.component" \
              "$HOME/Library/Audio/Plug-Ins/VST3/Rygostancija.vst3"; do
    if [ -d "$bundle" ]; then
        rm -rf "$bundle/Contents/Resources/Art"
        cp -R "$ROOT/art" "$bundle/Contents/Resources/Art"
        # Finder tags and download marks on the drawings stop codesign from signing the plugin.
        xattr -cr "$bundle/Contents/Resources/Art"
        codesign --force --deep --sign - "$bundle"
        echo "Updated: $bundle"
        updated=$((updated + 1))
    fi
done

if [ "$updated" -eq 0 ]; then
    echo "The plugin isn't installed. Run scripts/build.sh first"
    exit 1
fi
