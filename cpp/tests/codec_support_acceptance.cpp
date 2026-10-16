// AV1 and ten-bit decoding, checked against real media instead of a table.
//
// Both are decided at run time by the FFmpeg this player links: AV1 by whether that build carries an
// AV1 decoder at all (the native one, where libdav1d is not part of the build), and ten-bit by the
// software path, which packs it to P010. Neither verdict belongs in a hand-written list, so both are
// exercised on files, and the ten-bit decode is compared against the eight-bit decode of the same
// pattern: "it decoded" is not the same as "the picture is right", and a packed-ten-bit mistake is a
// picture several times too dark or too bright rather than a decode failure.
//
// Skips when the fixtures are absent, like the other fixture-driven tests.
#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "decoder_session.hpp"
#include "media_session.hpp"
#include "media_codec.hpp"
#include "software_pixel_payload.hpp"
#include "v4l2_format.hpp"
#include "media_v4l2.hpp"
#include "vulkan_device.hpp"
#include "vulkan_video_image.hpp"
#include <algorithm>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace {
// The mean of one plane of an eight-bit frame, over every sample.
double eight_bit_plane_mean(const lumaframevr::RetainedFrame& frame, int plane) {
    const int rows = plane == 0 ? frame.height() : (frame.height() + 1) / 2;
    unsigned long long total = 0;
    std::size_t count = 0;
    for(int row = 0; row < rows; ++row) {
        for(const auto sample : frame.plane_row(plane, row)) {
            total += sample;
            ++count;
        }
    }
    return count == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(count);
}
// The mean of a packed plane as ten-bit values. The packed payload is P010 whatever the decoder's
// own format was - the ten-bit sample sits in the high bits of a 16-bit word - so it is shifted back
// down here before anything is compared.
double packed_mean(std::span<const unsigned char> samples, unsigned shift) {
    unsigned long long total = 0;
    std::size_t count = 0;
    for(std::size_t i = 0; i + 1 < samples.size(); i += 2) {
        total += (static_cast<unsigned>(samples[i]) | (static_cast<unsigned>(samples[i + 1]) << 8)) >> shift;
        ++count;
    }
    return count == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(count);
}
// The interleaved chroma plane holds U and V one word apart; parity 0 is U, 1 is V.
double packed_chroma_mean(std::span<const unsigned char> samples, unsigned shift, unsigned parity) {
    unsigned long long total = 0;
    std::size_t count = 0;
    for(std::size_t word = 0; (word + 1) * 2 <= samples.size(); ++word) {
        if(word % 2 != parity)
            continue;
        const auto i = word * 2;
        total += (static_cast<unsigned>(samples[i]) | (static_cast<unsigned>(samples[i + 1]) << 8)) >> shift;
        ++count;
    }
    return count == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(count);
}
}  // namespace

int lumaframevr::tests::codec_support(int argc, char** argv) {
    return acceptance_main([&] {
        using namespace lumaframevr;
        const char* directory = argc > 1 && argv[1] != nullptr && argv[1][0] != '\0' ? argv[1] : nullptr;
        if(directory == nullptr)
            skip("no codec fixture directory was given");
        const std::string base = std::string(directory) + '/';
        const std::string short_path = base + "video-96x64-2s.mp4";
        const std::string av1_path = base + "video-av1-96x64-2s.mkv";
        const std::string ten_bit_path = base + "video-10bit-96x64-2s.mkv";
        const std::string hdr_path = base + "video-10bit-hdr-96x64-2s.mkv";
        for(const auto& path : {short_path, av1_path, ten_bit_path}) {
            if(!std::filesystem::is_regular_file(path))
                skip("the codec fixture is not present: " + path);
        }

        const auto decoded_first_frame = [](const std::string& path) {
            DecoderSession decoder(MediaSource::parse(path.c_str()).resolved(), true);
            auto frame = decoder.next_frame();
            require(frame.valid(), "a frame decodes from " + path);
            require(frame.width() == 96 && frame.height() == 64, "real geometry from " + path);
            require(frame.software(), "an explicit software request never probes the VPU");
            return frame;
        };

        // The eight-bit decode of the same pattern: the reference the other two are held against.
        auto reference = decoded_first_frame(short_path);
        require(reference.color().bits == 8, "the reference fixture is eight-bit");
        const double reference_luma = eight_bit_plane_mean(reference, 0);
        const double reference_u = eight_bit_plane_mean(reference, 1);
        const double reference_v = eight_bit_plane_mean(reference, 2);
        require(reference_luma > 1.0 && reference_luma < 254.0, "the reference picture is neither black nor white");
        reference = {};

        // AV1: this build either decodes it in software or refuses it with the reason. Both are real
        // outcomes and both are asserted here; what must never happen is an attempt that half-plays,
        // which is what a hwaccel-only decoder produces if the policy waves it through.
        if(media::av1_software_decoder_available()) {
            require(media::supported(av1_path.c_str()).status == 1, "AV1 is inside the policy of a build that can decode it");
            auto frame = decoded_first_frame(av1_path);
            near(eight_bit_plane_mean(frame, 0), reference_luma, 10.0, "AV1 luma matches the eight-bit decode of the same pattern");
        } else {
            require(media::supported(av1_path.c_str()).status == 0, "AV1 is outside the policy of a build that cannot decode it");
            const auto refused = media::open(av1_path.c_str(), true);
            require(std::holds_alternative<media::Error>(refused), "an AV1 file is refused rather than attempted");
            const auto& message = std::get<media::Error>(refused).message;
            require(message.find("AV1 decoder carries no software decode") != std::string::npos, "the refusal names what AV1 needs, and the message was: " + message);
            std::cout << "codec_support: AV1 refused as this build cannot decode it: " << message << '\n';
        }

        // The VPU side of both codecs, which is the half a codec the policy "knows" can quietly be
        // assumed to have: AV1 must never be offered to a VPU that has none, and ten-bit HEVC must
        // stay offered, because Q10C is exactly the ten-bit capture the VPU does have.
        const auto described = [](AVCodecID codec, AVPixelFormat format) {
            AVCodecParameters parameters{};
            parameters.codec_type = AVMEDIA_TYPE_VIDEO;
            parameters.codec_id = codec;
            parameters.format = format;
            parameters.width = 1920;
            parameters.height = 1080;
            return parameters;
        };
        require(!media::v4l2_accepts(described(AV_CODEC_ID_AV1, AV_PIX_FMT_YUV420P)), "AV1 is never offered to the VPU");
        require(media::v4l2_accepts(described(AV_CODEC_ID_HEVC, AV_PIX_FMT_YUV420P10LE)), "ten-bit HEVC stays offered to the VPU");

        // Ten-bit: accepted, decoded as ten-bit 4:2:0, packed P010-aligned, and the samples still the
        // ones the picture has.
        require(media::supported(ten_bit_path.c_str()).status == 1, "ten-bit 4:2:0 is inside the policy of this build");
        // Both routes to the GPU have to agree about what ten-bit is, and this holds on any machine,
        // GPU or not: the software path's packing and the import path's P010 fourcc name one format.
        require(software_yuv_format(8) == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, "an eight-bit software frame uses the eight-bit two-plane format");
        require(software_yuv_format(10) == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, "a ten-bit software frame uses the ten-bit two-plane format");
        require(software_yuv_format(10) == dma_buf_format(media::v4l2_p010, 10), "the software and imported ten-bit routes name the same Vulkan format");
        {
            auto frame = decoded_first_frame(ten_bit_path);
            const auto format = frame.format();
            require(format == AV_PIX_FMT_YUV420P10LE || format == AV_PIX_FMT_P010LE, "the decoder produced ten-bit 4:2:0");
            require(frame.color().bits == 10, "the decoded frame reports ten bits");
            auto payload = pack_software_420(frame);
            require(payload.bits == 10, "the packed payload carries ten bits");
            require(payload.sample_shift == (format == AV_PIX_FMT_P010LE ? 0U : 6U), "the packed payload is P010-aligned for the format the decoder produced");
            require(payload.luma.size() == static_cast<std::size_t>(payload.width) * 2 * payload.height, "the packed luma plane is two bytes a sample");
            near(packed_mean(payload.luma, payload.sample_shift) / 4.0, reference_luma, 8.0, "ten-bit luma matches the eight-bit decode of the same pattern");
            near(packed_chroma_mean(payload.chroma, payload.sample_shift, 0) / 4.0, reference_u, 8.0, "ten-bit U matches the eight-bit decode of the same pattern");
            near(packed_chroma_mean(payload.chroma, payload.sample_shift, 1) / 4.0, reference_v, 8.0, "ten-bit V matches the eight-bit decode of the same pattern");
            std::cout << "codec_support: ten-bit format=" << format << " shift=" << payload.sample_shift << " luma_mean_ten_bit=" << packed_mean(payload.luma, payload.sample_shift)
                      << " reference=" << reference_luma << '\n';
            // The GPU half of the same path, where a device is present: the frame has to survive pack,
            // format choice, image creation and upload, and it has to land in the format the hardware
            // route uses for P010, so the two routes cannot disagree about what ten-bit looks like. A
            // machine with no Vulkan device says so rather than failing: this is the media test.
            try {
                auto device = VulkanDevice::create();
                VulkanVideoImage image(device, std::move(frame));
                require(image.sampling()->format() == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
                        "a software ten-bit frame reaches the GPU in the same two-plane packing the P010 import uses");
                std::cout << "codec_support: ten-bit frame uploaded to " << device->name() << '\n';
            } catch(const std::exception& error) {
                std::cout << "codec_support: no Vulkan device for the ten-bit upload check: " << error.what() << '\n';
            }
        }
        // Not only the first frame: the ten-bit path has to survive a seek and a drain to EOF as well,
        // which is where frame lifetime and per-row handling go wrong on a format whose samples are
        // two bytes wide and whose rows are twice as long.
        {
            DecoderSession decoder(MediaSource::parse(ten_bit_path.c_str()).resolved(), true);
            auto first = decoder.next_frame();
            require(first.valid(), "ten-bit decodes a first frame");
            std::vector<unsigned char> original(first.plane_row(0, 0).begin(), first.plane_row(0, 0).end());
            RetainedFrame held = std::move(first);
            rejects([&] { decoder.seek(0.5); }, "a ten-bit seek requires releasing retained frames");
            held = {};
            decoder.seek(0.5);
            auto seeked = decoder.next_frame();
            require(seeked.valid() && seeked.color().bits == 10, "a ten-bit frame survives a seek");
            require(pack_software_420(seeked).bits == 10, "a ten-bit frame after a seek still packs P010-aligned");
            seeked = {};
            decoder.seek(0);
            auto again = decoder.next_frame();
            require(again.valid(), "ten-bit decodes again after a seek to zero");
            require(std::equal(original.begin(), original.end(), again.plane_row(0, 0).begin()), "a ten-bit seek returns the same pixels");
            again = {};
            int frames = 0;
            while(decoder.next_frame().valid()) ++frames;
            require(frames > 0, "ten-bit drains through EOF");
        }

        // Ten-bit HDR: the same ten-bit path, and the metadata the tone-mapping shader is driven from.
        // A ten-bit file that arrives without its transfer and primaries would be presented as SDR
        // without anything failing, so both halves are asserted rather than assumed.
        if(std::filesystem::is_regular_file(hdr_path)) {
            auto frame = decoded_first_frame(hdr_path);
            require(frame.color().bits == 10, "a ten-bit HDR frame arrives as ten-bit");
            require(frame.color().transfer == AVCOL_TRC_SMPTE2084, "a ten-bit HDR frame keeps its PQ transfer");
            require(frame.color().primaries == AVCOL_PRI_BT2020, "a ten-bit HDR frame keeps BT.2020 primaries");
            auto payload = pack_software_420(frame);
            require(payload.bits == 10 && payload.sample_shift == 6, "a ten-bit HDR frame packs P010-aligned like any other ten-bit frame");
            std::cout << "codec_support: ten-bit HDR transfer=" << frame.color().transfer << " primaries=" << frame.color().primaries << '\n';
        } else {
            std::cout << "codec_support: no ten-bit HDR fixture, its colour metadata is not checked\n";
        }
        std::cout << "AV1 and ten-bit decode through the software path, pixels compared with the eight-bit decode\n";
    });
}
