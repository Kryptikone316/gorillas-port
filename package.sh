#!/bin/bash
# Assembles the device package: package/Gorillas.sh + package/gorillas/{run.sh,gorillas.mipsel,gorillas.txt,README.txt,boxart.png}
# Run ./build_sf3500.sh first. Box art: tools/make_boxart.py (see its header).
set -e
cd "$(dirname "$0")"
OUT=package
rm -rf "$OUT"; mkdir -p "$OUT/gorillas"
cp package-src/Gorillas.sh "$OUT/"
cp package-src/{run.sh,gorillas.txt,README.txt} "$OUT/gorillas/"
cp build-sf3500/gorillas.stripped.mipsel "$OUT/gorillas/gorillas.mipsel"
[ -f package-src/boxart.png ] && cp package-src/boxart.png "$OUT/gorillas/"
chmod +x "$OUT/Gorillas.sh" "$OUT/gorillas/run.sh" "$OUT/gorillas/gorillas.mipsel"
(cd "$OUT" && find . -type f | sort | xargs md5sum)
