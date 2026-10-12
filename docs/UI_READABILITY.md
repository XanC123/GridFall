# UI Readability Pass

- Prefer Lato Medium, then Lato Semibold/Regular, then DejaVu Sans, Liberation Sans, or FreeSans. The built-in Raylib font remains the final fallback. No external font binary is bundled.
- The primary foreground is a soft warm-parchment (`#EFE5D0`), intentionally different from graphite/navy panels. Secondary text is muted blue-slate (`#B4C7D7`); amber/coral/mint/blue are used for semantic accents and selected/interactive states.
- Lato is loaded into a 64 px glyph atlas and sampled with bilinear filtering. The renderer uses a restrained dark shadow (not a sharp outline) for readability against variable 3D backgrounds.
- The minimum screen-space font size is 13 px and text measurement uses the same minimum and spacing so control labels are not clipped by a mismatch.
- The gameplay controls banner has its own translucent graphite backing so instructions remain readable over the 3D world.
- On Ubuntu/WSL, install the preferred typeface with `sudo apt install fonts-lato`. If it is not installed, the application falls back to other common sans-serif fonts.
