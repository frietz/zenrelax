#!/usr/bin/env bash
set -euo pipefail

DURATION=5
COLS=80
ROWS=24
CAST_DIR="assets/casts"
GIF_DIR="assets/gifs"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$CAST_DIR" "$GIF_DIR"

MODE_NAMES=(
  "" "plasma" "fractal" "particles" "quantum" "orbitals"
  "rainfall" "aurora" "starfield" "metaballs" "life"
)

for i in $(seq 1 10); do
  echo "Recording mode $i (${MODE_NAMES[$i]})..."
  python3 "$SCRIPT_DIR/record-cast.py" \
    --mode "$i" --duration "$DURATION" \
    --cols "$COLS" --rows "$ROWS" \
    --output "$CAST_DIR/mode${i}.cast"

  echo "Converting to GIF..."
  agg --cols "$COLS" --rows "$ROWS" \
    "$CAST_DIR/mode${i}.cast" \
    "$GIF_DIR/mode${i}.gif"
done

echo "Done. GIFs in $GIF_DIR/"
