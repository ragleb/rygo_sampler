#!/bin/bash
# Builds the plugin for Apple Silicon and Intel and installs it on this Mac.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

GENERATOR=()
command -v ninja >/dev/null && GENERATOR=(-G Ninja)

cmake -S "$ROOT" -B "$BUILD" "${GENERATOR[@]}" -DCMAKE_BUILD_TYPE=Release -DRYGO_UNIVERSAL=ON
cmake --build "$BUILD" --target Rygostancija_AU Rygostancija_VST3

"$ROOT/scripts/install_local.sh"
