# Dependency notices

LumaFrameVR is free software under the GNU GPL version 3 or later; the text is in
[../LICENSE](../LICENSE). 
The GPL covers the parts written for this project. Every dependency keeps its own terms, and those
terms travel with the component:

- FFmpeg 7 (LGPL-2.1-or-later): the player **links** the FFmpeg the frame already ships. This project
  neither builds nor distributes an FFmpeg of its own, so there is no build of ours whose sources
  would have to be offered; the libraries are still linked, so the licence texts stay.
- Mesa, the Vulkan driver the system provides (lavapipe on the development host): MIT.
- OpenVR header: `cpp/application/model_vendor/LICENSE`, BSD-3-Clause, Valve Corporation.
- Vulkan loader and the OpenXR headers: Apache-2.0.
- PulseAudio, glibc and libstdc++ (GPL-3.0 with the GCC Runtime Library Exception): their own
  terms, unchanged. libstdc++'s exception is what lets a program link it at all.
- Controller test assets: `cpp/application/model_assets/LICENSE.md`.
- Eye artwork: `assets/eye/` ships its ISC license and provenance.
- Inter 4.0 (typeface), baked into `cpp/ui/assets/font_*.hpp` by `scripts/make-font-atlas.py`:
  SIL Open Font License 1.1, see `Inter-OFL.txt`. The baked glyph bitmaps are a derivative of
  the font and stay under that licence, which is why the licence travels with the payload.
- Lucide (icon set), baked into `cpp/ui/assets/icons.hpp` by `scripts/make-icon-atlas.py`:
  ISC, see `Lucide-ISC.txt`.

A binary release must carry the notices and the corresponding sources for the exact
configurations it ships. What the payload ships is the project's own code, the OpenVR API
library, the branding and the baked typeface and icon atlases; the packaging step bundles the
project's own source tree as `source/`. The FFmpeg the player links is not among them — it is the
frame's, and its sources are the frame's too.

## What it runs on

The operating system underneath is the user's business, but for the record:

- **SteamOS** is Arch Linux plus Valve's packages, and it has no single license. Each package
  carries its upstream license in its `PKGBUILD`, and Valve publishes the sources of what it
  ships through its package mirror. An OS full of GPL packages does not put the applications
  that run on it under the GPL.
- **The Linux kernel** is GPL-2.0 only, with an explicit syscall note. User-space programs do
  not link it.
- **SteamVR** is two things. The OpenVR SDK this project uses is BSD-3-Clause, so it links into
  any license. The runtime and the Steam client are Valve's proprietary software under the
  Steam Subscriber Agreement, which covers personal, non-commercial use and points commercial
  deployments at Valve's enterprise programme. They are not distributed with this project: the
  player only talks to whatever OpenXR runtime the machine provides, the same way an OpenGL
  program talks to a vendor driver. That is why a GPL-3.0 application can run on it.

