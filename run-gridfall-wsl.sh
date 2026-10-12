#!/usr/bin/env bash
set -euo pipefail

# WSLg: prefer the D3D12-backed NVIDIA adapter instead of llvmpipe software rendering.
# These variables only affect Gridfall launched by this script.
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
unset LIBGL_ALWAYS_SOFTWARE || true

# The patched Raylib SDL backend chooses the warp fallback on X11 (WSLg/XWayland)
# and leaves native Wayland relative motion alone. Override with
# GRIDFALL_SDL_RELATIVE_MOUSE_WARP=0 or =1 when comparing backends.

if [[ ! -x "build/gridfall" ]]; then
    echo "Gridfall executable not found. Build first with: cmake --build build --parallel" >&2
    exit 1
fi

exec ./build/gridfall "$@"
