#!/usr/bin/env bash
set -euo pipefail

# WSLg: prefer the D3D12-backed NVIDIA adapter instead of llvmpipe software rendering.
# These variables only affect Gridfall launched by this script.
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
unset LIBGL_ALWAYS_SOFTWARE || true

# SDL2's raw relative-motion path can return virtual-pointer-sized deltas under
# WSLg/XWayland. Warp-based relative mode avoids that broken raw-input path.
export SDL_MOUSE_RELATIVE_MODE_WARP="${SDL_MOUSE_RELATIVE_MODE_WARP:-1}"

if [[ ! -x "build/gridfall" ]]; then
    echo "Gridfall executable not found. Build first with: cmake --build build --parallel" >&2
    exit 1
fi

exec ./build/gridfall "$@"
