# Camera passthrough eye

The unmodified `eye.svg` was fetched from Lucide at the pinned revision and URLs in `provenance.json`. The original upstream `LICENSE` is retained in full; the eye is licensed under ISC. Preserve the copyright and permission notice when distributing the embedded raster or artwork.

Both native C++ browser and transport controls use the same embedded alpha mask in `cpp/ui/assets/passthrough_eye.hpp`. No runtime SVG loader, image package, network access, or camera implementation is added.

Reproduce the mask from the retained SVG with:

```
uv run --with cairosvg --with pillow python assets/eye/rasterize.py
```

The host-only rasterizer uses 160x160 supersampling and a 40x40 Lanczos-downsampled alpha mask. `eye-raster.png` retains the supersampled source raster. CairoSVG and Pillow are not application dependencies.
