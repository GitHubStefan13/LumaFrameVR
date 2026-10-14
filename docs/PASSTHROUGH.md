# Beam stereo and camera passthrough

Controller models, tracked aim/grip poses, stereo body geometry and world-space beam surfaces have distinct rendering paths. Camera controls use advertised runtime capabilities, not a synthetic background.

## Camera passthrough

One shared Controller state drives the sourced Lucide eye buttons in both the file viewer and video controls. Both are off by default; enabled state is blue, unsupported state is dim. Genuine Lucide SVG, ISC license and upstream provenance are included in assets/eye.

The backend queries the runtime's environment blend modes and, when advertised, enables XR_FB_passthrough and checks supportsPassthrough. Supported native alpha/additive environment blending or an FB reconstructed camera layer can expose surroundings. Reconstruction resources are acquired only after the user enables the feature, started/resumed together, paused on disable/suspension, and destroyed layer-first through RAII. Camera errors disable the request and report a diagnostic. A restored capability does not silently restart cameras.

Camera underlay is below the normal stereo scene, then UI/preview and foreground beams. With passthrough enabled the empty projection/video background is transparent; media and controller surfaces remain drawn. Stereo-eye selection is masked independently of the transparent-background flag. With passthrough off the accepted opaque black background returns. Native additive blending cannot provide fully opaque dark video pixels; this is recorded in its backend diagnostic.

Runtime capability is not established by compiling extension headers. If SteamVR on the Frame exposes only opaque blending and no usable XR_FB_passthrough, the eye is disabled and no camera background is claimed or fabricated. No third-party API layer is installed automatically, no camera footage is synthesized, and no headset is contacted by delivery verification.

Startup diagnostics include Passthrough backend/reason and advertised modes in ~/.local/state/lumaframevr/startup.log. Physical camera alignment, permission, binocular beam fusion and compositor compatibility require testing the Release candidate on the device by the user.
