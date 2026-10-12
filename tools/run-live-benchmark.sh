#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Always configure the game, tests, and frame-report tool. If Raylib was already
# fetched in build/_deps this reuses it; otherwise FetchContent downloads it.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGRIDFALL_BUILD_GAME=ON \
  -DGRIDFALL_FETCH_RAYLIB=ON \
  -DGRIDFALL_BUILD_TESTS=ON \
  -DGRIDFALL_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
mkdir -p build/performance

cat <<'PROMPT'
Live Gridfall frame capture:
  1. Keep the F10 diagnostics graph closed for minimal overhead.
  2. Play a repeatable sector/combat scene for 30-60 seconds.
  3. Close the game window normally; the game exports its last 4096 frame samples.
The report below separates frame time, update CPU, render-submit CPU, present wait,
and input-poll-to-render-submit proxy. The last metric is NOT input-to-photon latency.
PROMPT

./run-gridfall-wsl.sh

if [[ -s build/performance/gridfall-frame-samples.csv ]]; then
  ./build/gridfall_frame_report build/performance/gridfall-frame-samples.csv
else
  echo "No frame samples were written. Press F11 in-game or close the game cleanly." >&2
  exit 1
fi
