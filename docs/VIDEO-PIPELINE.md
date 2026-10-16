# Video pipeline

## Decode and ownership

A local file or supported remote media source is resolved by `cpp/source/`.
The native FFmpeg backend demuxes streams and feeds bounded packet read-ahead.
`cpp/media/DecoderSession` owns the decoder and returns retained frame objects;
`cpp/playback/` handles asynchronous decoding, timestamps, pacing, seeking and
published state. PulseAudio output stays in the native audio backend.

The hardware path receives DRM PRIME/DMA-BUF frames from the target decoder.
Vulkan imports their external storage with the advertised plane layout and DRM
modifier. Frame retention and submission synchronization protect that storage
until the GPU has finished using it. Missing decoder/import support is an error,
not proof of a working device pipeline.

The software compatibility path handles supported CPU video planes through its
own upload route, in eight- and ten-bit 4:2:0: ten-bit is packed to P010 and
uploaded in the two-plane format the import path uses for P010 frames. AV1 is
decoded by the software AV1 decoder the linked FFmpeg carries, `libdav1d` or
`libaom-av1`: FFmpeg 7's native `av1` entry returns ENOSYS from its own format
negotiation without a hardware accelerator, so a build whose only AV1 entry is
that shell refuses an AV1 file where it is opened, with that reason, instead of
starting a decode that cannot produce a frame. Which of the two a build is, is
asked at run time rather than assumed: the frame's own FFmpeg carries libdav1d
and plays AV1, while the AArch64 SDK copy taken off it says
`CONFIG_LIBDAV1D_DECODER 0` - the copy is not authoritative about the device, and
the probe is what decides. Browser thumbnails and seek previews are separate
workloads; small preview decoding must not be described as full-frame hardware
playback.

## Colour and projection

Native frame metadata carries range, matrix, transfer, primaries, bit depth and
chroma information. `cpp/graphics/` validates import/sampling support and the
production shaders perform colour conversion and PQ/HLG HDR-to-SDR tone mapping.
This is SDR presentation, not native HDR display output.

Preserve the coded allocation extent when importing padded frames; visible crop
belongs in texture coordinates. Preserve ten-bit content through the applicable
plane layout and sampler format: `software_yuv_format` and `dma_buf_format` name
one format for ten-bit, and the software packer P010-aligns every ten-bit plane
it carries, so an eight-bit picture cannot be presented as ten-bit or the other
way round. A ten-bit format the packer has no shift for (big-endian ten-bit,
4:2:2, 12-bit) is refused where the file is opened, with the reason. Do not
silently relabel an eight-bit import as a ten-bit path or introduce a full-frame
CPU colour-conversion fallback to satisfy an unsupported host GPU test.

`cpp/presentation/` resolves flat, 180-degree, 360-degree and fisheye projection,
mono/SBS/top-bottom layout, eye order, Full/Half formats and alignment. Explicit
CLI overrides, supported metadata/filename hints and per-video remembered state
have distinct precedence. Projection changes do not require decoder reopening.

## Verification boundary

Media, timeline, ownership, format, import, sampler, colour and rendering contracts
are registered in CTest. Tests with a software Vulkan implementation cover genuine
GPU execution where supported; they do not establish Steam Frame VPU/DMA-BUF,
Main10/HDR appearance, audio synchronization, display latency or sustained 8K
performance. Unsupported production YCbCr device support is an explicit skip: the
lavapipe used on the build host advertises no multi-planar sampling at all (neither
`G8_B8R8_2PLANE_420` nor `G10X6_B10X6R10X6_2PLANE_420_3PACK16` has any optimal-tiling
feature), so the ten-bit upload to the GPU is a device check while the ten-bit
samples themselves, the P010 alignment and the format both routes pick are checked
on the host.

## Checking these two on the frame

The player's own log is the first answer, because it links the frame's FFmpeg, and the device run of
a file is the authority: the AArch64 SDK copy taken off the frame reports
`CONFIG_LIBDAV1D_DECODER 0` and no dav1d strings, while the frame's own FFmpeg carries libdav1d and
plays AV1 through it - the player says which decoder it used. Play `video-av1-96x64-2s.mkv`, which
`cpp/tests/init-test-environment.sh` writes beside the other samples: it should play, and a build
without a software AV1 decoder refuses it with that reason instead.

Ten-bit is a device check for its hardware half, because the samples, their P010 alignment and the
format both routes name are checked on the host, while the appearance on the headset and the VPU's
own Q10C capture are not. The generated `video-10bit-96x64-2s.mkv` is the smallest thing to play
first: read `Decoded frame: ... depth=10` in the log. For the VPU's own ten-bit path, play a ten-bit
4K HEVC file with `LUMAFRAMEVR_DECODER=v4l2`, where Q10C and the vendor modifier are what the queue
negotiates; Vulkan's memory requirement for that modified capture runs a few pages above what the
driver allocated, so what is imported is the negotiated layout whose described planes fit inside the
allocation, with the difference logged. A layout whose planes overrun the buffer is still refused.

Use the test suites and per-release evidence for measured results.
Do not carry performance numbers from a different application or device build
into current acceptance claims.
