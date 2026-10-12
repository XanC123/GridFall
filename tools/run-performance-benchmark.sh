#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p build/performance
CXX="${CXX:-g++}"
COMMON=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wshadow -Werror -Iinclude)
DOMAIN=(
  src/world/Level.cpp
  src/entities/Inventory.cpp
  src/systems/ProgressionSystem.cpp
  src/systems/VfxSystem.cpp
  src/systems/CombatSystem.cpp
  src/systems/EnemySystem.cpp
)

"$CXX" "${COMMON[@]}" tests/core_tests.cpp "${DOMAIN[@]}" src/systems/SaveSystem.cpp \
  -o build/gridfall_core_tests
"$CXX" "${COMMON[@]}" tests/performance_benchmark.cpp "${DOMAIN[@]}" \
  -o build/gridfall_performance_benchmark
"$CXX" "${COMMON[@]}" tools/analyze-frame-csv.cpp \
  -o build/gridfall_frame_report

./build/gridfall_core_tests
./build/gridfall_performance_benchmark build/performance/simulation-benchmark.csv
