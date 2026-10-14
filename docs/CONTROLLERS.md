# Genuine runtime controller models

Controller bodies render in the ordinary stereo scene with real eye poses/FOV, OpenXR grip poses, indexed geometry, per-part textures and a depth attachment. Foreground beams use separate world-space surfaces; bodies are not painted onto a fixed near-plane overlay. See [Passthrough and beam composition](PASSTHROUGH.md).

## Model source

The application starts an OpenVR background session asynchronously and reads the connected controller roles and actual render-model property. SteamVR's IVRRenderModels supplies the attached hardware's genuine models; no guessed Steam Frame filename or white-box/Index fallback is used. Multipart bodies/buttons and their separate materials/neutral visibility are retained. The model's OpenXR grip component is composed into geometry before real OpenXR grip tracking is applied. Missing alignment, missing runtime or model/texture errors are logged rather than guessed.

Release test6 includes Valve's official AArch64 libopenvr_api.so from OpenVR commit 0924064316de3effbcd1acf1e309182a2deb1c05, bin/linuxarm64/libopenvr_api.so. SHA256 cc3671d24dd23fb61494e8a8327cf4fe7f638a60508ff7225984a03f2f5947e9. It is the untouched redistributable BSD-3-Clause SDK library; the license and source URL accompany it. The application resolves it beside the packaged shaders, so no user asset export or Frame access by the assistant is required.

Internet research did not establish an independently redistributable exact Steam Frame mesh. Licensed Valve Index assets are strictly test fixtures, identified as such; they are never selected by the deployed player. The source includes their upstream MIT license, original GLBs and reproducible converter. The test-only fake ABI library is not shipped as the runtime SDK.

## Verification

Production Vulkan tests cover indexed texture/depth, hand routing, visibility, missing-grip rejection and stereo disparity. Licensed controller fixtures are uploaded and rendered in both eyes; they are not substitutes for runtime hardware models. They are not acceptance evidence for a physical headset.

## Remaining physical gates

Actual Steam Frame model retrieval, grip alignment and binocular appearance require user testing. Controller components are initially neutral, without live button animation. Compressed runtime textures are explicitly unsupported; RGBA8-sRGB is supported. Missing grip/component data results in a diagnostic and no fabricated body, while beams/input remain functional. Steam Library startup, VPU/DMA-BUF/Main10/HDR/audio and sustained latency retain their previous unverified gates. No Frame was contacted, read, installed to or launched.

The launch log records controller hand, status, actual model name, component count and error details at ~/.local/state/lumaframevr/startup.log.
