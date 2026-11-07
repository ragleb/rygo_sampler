#!/bin/bash
# Builds the archive for distribution from the latest build (run scripts/build.sh first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARTEFACTS="$ROOT/build/Rygostancija_artefacts/Release"
VERSION="$(sed -n 's/^project(Rygostancija VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
NAME="Rygostancija-$VERSION-mac"
STAGE="$ROOT/dist/$NAME"

rm -rf "${STAGE:?}" "${STAGE:?}.zip"
mkdir -p "$STAGE"

cp -R "$ARTEFACTS/AU/Rygostancija.component" "$STAGE/"
cp -R "$ARTEFACTS/VST3/Rygostancija.vst3" "$STAGE/"
cp "$ROOT/scripts/install_mac.command" "$STAGE/Install.command"
chmod +x "$STAGE/Install.command"

cat > "$STAGE/Read Me.txt" <<'EOF'
Rygostancija by ZHRI Records
A sampler plugin for Logic Pro, REAPER and other AU or VST3 hosts on macOS 11 or newer.

Installing
1. Unpack the archive.
2. Right-click "Install.command" and choose Open.
   If macOS refuses to open it: System Settings > Privacy & Security > Open Anyway.
3. Restart Logic or REAPER and look for the plugin under ZHRI Records.

Using it
Long samples sit on the low keys, short ones on the high keys.
Drag a knob with the mouse. Shift: fine control. Double-click: reset. Click a number: type a value.
The EFFECTS switch turns all effects off, leaving a plain sampler.
ONE SHOT always plays to the end. GATE plays while the key is held. LOOP starts again from the beginning while the key is held.

Music mode
Turn on MUSIC, then play a key or pick a sample from the list.
Close the Sample Lock: that sample is now on every key, in tune, starting from its root note.
Open the lock to go back to choosing.
EOF

ditto -c -k --keepParent "$STAGE" "$STAGE.zip"
ls -la "$STAGE.zip"
