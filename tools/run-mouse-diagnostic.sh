#!/usr/bin/env bash
set -euo pipefail

# Run the interactive visual center-relative pointer probe. The game has its own automatic 10-second terminal trace.
export GALLIUM_DRIVER=d3d12
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
unset LIBGL_ALWAYS_SOFTWARE || true

# The probe uses Raylib's ordinary absolute pointer path. MouseInput hides and
# recenters the pointer without enabling SDL relative mode.

if [[ ! -x build/gridfall_mouse_probe ]]; then
    echo "Mouse probe not built. Configure with -DGRIDFALL_BUILD_MOUSE_PROBE=ON and build first." >&2
    exit 1
fi
exec ./build/gridfall_mouse_probe "$@"
