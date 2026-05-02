#!/bin/bash
# Rygo Sampler — Mac Installer
# Double-click this file in Finder to install

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

AU_SRC="$SCRIPT_DIR/rygo_sampler.component"
VST3_SRC="$SCRIPT_DIR/rygo_sampler.vst3"

AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"

echo "=== Rygo Sampler Installer ==="
echo ""

install_plugin() {
    local src="$1"
    local dest_dir="$2"
    local name="$(basename "$src")"

    if [ ! -e "$src" ]; then
        echo "  [skip] $name not found next to installer"
        return
    fi

    mkdir -p "$dest_dir"

    if [ -e "$dest_dir/$name" ]; then
        echo "  Removing old version: $dest_dir/$name"
        rm -rf "$dest_dir/$name"
    fi

    cp -r "$src" "$dest_dir/"
    # Remove macOS quarantine flag so the plugin opens without warnings
    xattr -rd com.apple.quarantine "$dest_dir/$name" 2>/dev/null

    echo "  Installed: $dest_dir/$name"
}

install_plugin "$AU_SRC"   "$AU_DEST"
install_plugin "$VST3_SRC" "$VST3_DEST"

echo ""
echo "Done! Restart your DAW to see Rygo Sampler."
echo ""
read -p "Press Enter to close..."
