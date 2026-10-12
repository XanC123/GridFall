#!/usr/bin/env bash
set -euo pipefail

# Run the interactive visual raw-axis probe. The game has its own automatic 10-second terminal trace.
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
unset LIBGL_ALWAYS_SOFTWARE || true

# The patched backend defaults to native/raw relative input. Use
# GRIDFALL_SDL_RELATIVE_MOUSE_WARP=1 only to compare the legacy warp fallback.

if [[ ! -x build/gridfall_mouse_probe ]]; then
    echo "Mouse probe not built. Configure with -DGRIDFALL_BUILD_MOUSE_PROBE=ON and build first." >&2
    exit 1
fi
exec ./build/gridfall_mouse_probe "$@"
