# Application architecture

One C++20 application, built by the CMake project in the root. The backend talks to
FFmpeg and PulseAudio. GLSL shaders run through Vulkan and OpenXR. Python appears only in the
Steam shortcut helper and in the developer tooling; it never touches playback or rendering.

## Ownership and concurrency

Decoders and retained frames are move-only, so a frame stays alive until the last owner lets
go of it, including while a graphics submission still references it. Decoding, packet
reading, directory scanning, thumbnail generation and controller-model loading all run off the
XR render loop, and every queue and cache has a bound. Failures surface as a diagnostic
instead of unwinding the frame.

Controller models load on their own worker and publishing a snapshot is atomic, so rendering
never blocks on a model that is still loading. Native resources outlive the models, textures
and submissions that depend on them.

Settings and alignment resolve separately from decoding, so changing projection or stereo does
not reopen the decoder. The browser, transport and advanced-dialog controls use the same code
the acceptance tests exercise.

## Rendering and installation

The renderer uses the real eye transforms and the tracked aim and grip spaces. Scene geometry,
video projections, panel images and the world-space beam surfaces each own their submission
path. Camera resources are capability-gated: a runtime without a passthrough backend gets a
disabled control and a diagnostic rather than a guess.

Installation does not depend on the application running. The installer keeps an immutable
versioned release under `~/.local/opt/lumaframevr/releases` and points one permanent launcher
and a `current` symlink at it, so an upgrade or a repair never rewrites the payload that is
running. It removes only the paths it recorded itself and refuses a release directory it did
not create. See [Installation](INSTALLATION.md), [Controller models](CONTROLLERS.md),
[Passthrough](PASSTHROUGH.md), [Video pipeline](VIDEO-PIPELINE.md).
