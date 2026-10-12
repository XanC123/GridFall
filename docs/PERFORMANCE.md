# Gridfall performance testing

Gridfall includes two complementary measurements: a deterministic headless CPU benchmark and a live graphical frame capture. They answer different questions; do not interpret the headless numbers as an FPS estimate.

## 1. Headless simulation stress benchmark

Run from the project root:

```bash
./tools/run-performance-benchmark.sh
```

The script compiles with `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wshadow -Werror`, runs gameplay regression tests, then measures 20,000 60-Hz simulation ticks after warm-up. The scenario uses Sector 20 tuning with 27 enemies kept alive in a clear arena, sustained ranged AI/projectiles, player bullets, and VFX. It also writes per-tick sample data to `build/performance/simulation-benchmark.csv` and a text summary beside it.

Latest run in the artifact-building environment:

| Metric | Result |
|---|---:|
| Simulation CPU mean | 0.0033 ms/tick |
| p50 | 0.0031 ms/tick |
| p95 | 0.0045 ms/tick |
| p99 | 0.0050 ms/tick |
| Max observed tick | 0.2281 ms/tick |
| Ticks above 6.944 ms (144-Hz budget) | 0 / 20,000 |
| Ticks above 10.42 ms (spike threshold) | 0 / 20,000 |
| Ticks above 16.667 ms (60-Hz budget) | 0 / 20,000 |

The maximum is a rare scheduler/measurement outlier and is not reflected in p99. These measurements came from the build environment, not the user's laptop, and they exclude Raylib drawing, GPU work, WSLg, compositor scheduling, and display presentation. They indicate that the domain simulation itself is very light in that environment; use the live capture below to localize the actual 80–90 FPS behavior.

Projectile simulation reuses member-owned scratch vectors for survivors so that once buffers have grown to workload size, the update path does not allocate/deallocate a fresh temporary vector on every 60-Hz tick.

## 2. Live graphical frame capture on WSLg

First build the graphical application in Release mode. If you already configured and built Gridfall in `build/`, reconfigure and rebuild after pulling this source update:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGRIDFALL_FETCH_RAYLIB=ON \
  -DGRIDFALL_BUILD_TESTS=ON \
  -DGRIDFALL_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Then run the live test:

```bash
./tools/run-live-benchmark.sh
```

Play for 30–60 seconds in a repeatable scenario (same sector, move and shoot, leave F10 detail overlay closed), then close the game normally. The game writes its rolling last-4096 frame samples to `build/performance/gridfall-frame-samples.csv`; the script prints mean, p95, p99, max, and spike counts for frame period, update CPU time, render-submission CPU time, `EndDrawing()`/presentation wait, and the input-poll-to-render-submit CPU proxy.

- At 144 Hz, the frame period target is 6.944 ms.
- The report counts frame spikes above 10.42 ms (1.5× that budget).
- Press `F10` for the in-game diagnostics panel; avoid using that expanded panel while collecting the cleanest benchmark because it draws a graph and computes percentiles every frame.
- Press `F11` for an early CSV export; a clean game exit exports the rolling samples automatically.
- Compare modes with `./tools/run-live-benchmark.sh` and `GRIDFALL_VSYNC=1 ./tools/run-live-benchmark.sh`.

`present_ms` is the duration around Raylib's `EndDrawing()`, which can include frame pacing/presentation waits. `input_to_submit_ms` is a CPU-side time from input polling to the end of renderer submission. It is **not** physical input-to-photon latency: measuring that requires high-speed video, a photodiode, or other external instrumentation.

## 3. How to interpret the result

- High `update_ms`: inspect game logic/simulation.
- High `render_cpu_ms`: reduce draw-call count, per-frame loops, material changes, or geometry detail.
- Low CPU values but high `present_ms`: investigate WSLg/compositor/frame pacing/VSync behavior.
- Frame time high with all the above low: look at waits outside the measured intervals, driver scheduling, or variability in the display pipeline.

The NVIDIA WSL launcher sets `GALLIUM_DRIVER=d3d12` and `MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA`, and unsets `LIBGL_ALWAYS_SOFTWARE`, so the app uses the accelerated D3D12-backed Mesa renderer when that adapter is available.
