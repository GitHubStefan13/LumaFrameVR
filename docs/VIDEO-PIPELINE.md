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
own upload route. Browser thumbnails and seek previews are separate workloads;
small preview decoding must not be described as full-frame hardware playback.

## Colour and projection

Native frame metadata carries range, matrix, transfer, primaries, bit depth and
chroma information. `cpp/graphics/` validates import/sampling support and the
production shaders perform colour conversion and PQ/HLG HDR-to-SDR tone mapping.
This is SDR presentation, not native HDR display output.

Preserve the coded allocation extent when importing padded frames; visible crop
belongs in texture coordinates. Preserve ten-bit content through the applicable
plane layout and sampler format. Do not silently relabel an eight-bit import as
a ten-bit path or introduce a full-frame CPU colour-conversion fallback to satisfy
an unsupported host GPU test.

`cpp/presentation/` resolves flat, 180-degree, 360-degree and fisheye projection,
mono/SBS/top-bottom layout, eye order, Full/Half formats and alignment. Explicit
CLI overrides, supported metadata/filename hints and per-video remembered state
have distinct precedence. Projection changes do not require decoder reopening.

## Verification boundary

Media, timeline, ownership, format, import, sampler, colour and rendering contracts
are registered in CTest. Tests with a software Vulkan implementation cover genuine
GPU execution where supported; they do not establish Steam Frame VPU/DMA-BUF,
Main10/HDR appearance, audio synchronization, display latency or sustained 8K
performance. Unsupported production YCbCr device support is an explicit skip.

Use the test suites and per-release evidence for measured results.
Do not carry performance numbers from a different application or device build
into current acceptance claims.
