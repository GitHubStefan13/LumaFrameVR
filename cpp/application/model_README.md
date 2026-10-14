# Runtime controller models and licensed GPU fixtures

## Public API

Headers: `model_data.hpp`, `model_runtime.hpp`, namespace `lumaframevr::application`.

`RuntimeControllerModels loader(optional_library_path);` starts one worker. `loader.poll()` only reads the latest immutable snapshot: it never calls OpenVR, waits for model loading, or copies vertex/texture buffers. `RuntimeModelSnapshot::hands[0]` is left, `[1]` is right. Each `HandModelSnapshot` has `status`, diagnostic `detail`, and `shared_ptr<const RuntimeControllerModel> model`.

`RuntimeControllerModel` has root `name`, `provenance`, `vector<ControllerModel> parts`, and `grip_aligned`. Each `ControllerModel` has 32-byte `ModelVertex` position3/normal3/uv2 arrays, uint16 triangle indices, width/height, tightly packed RGBA8-sRGB top-down base-level texture bytes, name/provenance, neutral-state `visible`, and optional column-major `grip_from_model` 4x4.

When `grip_aligned` is true, all part positions and normals are already OpenXR grip-local. The component composition is inverse(openxr_grip.mTrackingToComponentLocal) times component.mTrackingToComponentRenderModel. `grip_from_model` is identity because the geometry is already baked. Draw with the application's real OpenXR grip pose; do not apply an extra controller offset or invert again. Respect each part's `visible` flag and individual texture. UVs/indices are unchanged.

When status is `MissingGrip`, parts are raw root-model-local, `grip_aligned` is false, and `grip_from_model` is absent. No guessed alignment is supplied. Do not silently treat those parts as grip-local. Component-free roots load as a single mesh; component hierarchies enumerate every renderable part and retain separate materials, including neutral-hidden parts. Buttons are loaded at neutral state; live input animation is intentionally outside this API.

Statuses: `Starting`, `Unavailable`, `Searching`, `LoadingModel`, `LoadingTexture`, `Ready`, `MissingGrip`, `Error`. Errors carry diagnostics; no procedural/fake fallback is ever returned. Per-device load failures persist until disconnect or model/device identity changes. Missing controllers are polled again for hotplug. Runtime initialization failures require recreating the loader after fixing runtime setup.

The dynamic SDK session initializes `VRApplication_Background`, requests only `FnTable:IVRSystem_026` and `FnTable:IVRRenderModels_006`, and calls only controller-role, connection, property, model, texture and component APIs. It never requests compositor, tracking poses, actions or haptics. Same-path loaders share initialization; runtime mesh/texture owners retain their session through RAII freeing. Published models own copied data and outlive the loader. Destruction requests worker stop and joins it.

Compile all `model_*.cpp` in this directory with C++20, SYSTEM include `model_vendor`, and link the platform dynamic-loader library plus threads. Do not compile test fake runtime sources into the app. No compile-time OpenVR binary link is required. Release test6 packages Valve’s official Linux ARM64 SDK and resolves it executable-adjacent; otherwise the deployed runtime must supply `libopenvr_api.so`. See [Controller models](../../docs/CONTROLLERS.md) for exact commit, binary hash and integration gates.

## Real permissively licensed fixtures

`model_assets/left.glb` and `right.glb` are untouched actual Valve Index controller assets, not Steam Frame assets. Source repository: https://github.com/immersive-web/webxr-input-profiles at commit `f4992299601614adbfefd398dc8e281556bb7444`, paths `packages/assets/profiles/valve-index/left.glb` and `right.glb`. The package README explicitly licenses its assets under MIT. Original MIT copyright/license is included at `model_assets/LICENSE.md` (Copyright 2019 Amazon). Device trademarks are not licensed for endorsement or alternate skins.

`model_assets/valve_index_left.lcm` and `valve_index_right.lcm` are GPU-ready derived fixtures. `load_controller_fixture(path)` loads them; `validate_controller_model(model)` returns an empty string on success or a diagnostic on failure. Original scene/node transforms are baked in meters, all six mesh nodes (body, A/B buttons, thumbstick, touchpad, trigger) retained. Shared source base-color material is retained. The original embedded RGB PNG is decoded by a standard-library-only converter and nearest-neighbor reduced from 2048x2048 to 512x512 RGBA8. Normals are transformed and normalized. The baked scene coordinates use the upstream WebXR controller grip-origin convention; fixture matrix is identity.

Left: 7,494 vertices, 26,778 indices. Right: 7,507 vertices, 26,799 indices. Each texture has 1,048,576 bytes. Fixture hashes: left `f9291ab1e0580ea79477bfcf7a09d858cccd03b7f12fc45d5c77b5b9f0928071`, right `6351377b41b72df029ed3d7cd45c64f0af6bfd41e4009396faf40f363b312a71`.

`convert_fixture.py input.glb new_output.lcm` reproduces the fixtures without third-party packages. It uses exclusive creation and never overwrites source/output. LCM format: 8-byte LFCMODEL magic; four little-endian uint32 vertex count/index count/width/height; uint32-length UTF-8 provenance and name; uint32 matrix-present flag followed by 16 float32 if present; vertex float32 position3/normal3/uv2; uint16 indices; RGBA bytes. Reader checks dimensions, truncation, trailing data, index ranges and finite values.

## OpenVR SDK provenance

`model_vendor/openvr_capi.h` is an untouched official Valve OpenVR 2.15.6 header from commit `0924064316de3effbcd1acf1e309182a2deb1c05`; SHA256 `3d32d8eb5bcbe2e250610123aee8142c68f0e7d4da823f87513b6a2a333078d7`. The official BSD-3-Clause license is included at `model_vendor/LICENSE`. Untouched licensed third-party header exceeds the project's source-line limit; every authored C++ file stays below 200 lines.

Only RGBA8_SRGB runtime textures are accepted; compressed runtime formats produce an explicit error rather than misinterpreted pixels. Real Steam Frame assets depend on the deployed SteamVR driver and are not obtainable or verified on this host.

## Isolated verification

Standalone project: `model_tests/CMakeLists.txt`. Configure Release into a separate build directory, build with at most two jobs, and run CTest. The suite includes real-source fixture validation, missing-library behavior, an explicitly labeled test-only ABI runtime using real fixture data, component transforms/materials, missing grip, disconnect, SDK failure cases, cancellation and shared-session lifetimes. The fake runtime is not evidence of live SteamVR success.

The host has no OpenVR path registry or physical controller runtime. Official SDK initialization can therefore report error 110 (VRInitError_Init_PathRegistryNotFound). Local fixtures and fake-ABI checks do not prove runtime model retrieval, grip alignment or headset appearance. Local fixtures do not establish runtime model retrieval or headset appearance.
