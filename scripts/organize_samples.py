#!/usr/bin/env python3
"""Compresses the source WAV files into FLAC without loss, for shipping inside the plugin.

The source files are not changed. The plugin itself sorts the samples by length.

Usage:
  python3 scripts/organize_samples.py <folder_with_wav> <destination_folder>
"""
import glob
import os
import subprocess
import sys


def main():
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    count = 0
    for path in sorted(glob.glob(os.path.join(src, "*.wav"))):
        stem = os.path.splitext(os.path.basename(path))[0]
        out = os.path.join(dst, stem + ".flac")
        subprocess.run(["flac", "-8", "--silent", "--force", "-o", out, path], check=True)
        count += 1
    print(count, "samples")


if __name__ == "__main__":
    main()
