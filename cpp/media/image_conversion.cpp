#include "image_conversion.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
}
namespace lumaframevr {
ImagePixels image_rgba(const AVFrame& frame, const std::function<bool()>& cancelled) {
    const auto* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame.format));
    if(!desc || (desc->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_FLOAT)))
        throw std::runtime_error("Unsupported image pixel representation");
    const bool palette = desc->flags & AV_PIX_FMT_FLAG_PAL;
    const bool rgb = desc->flags & AV_PIX_FMT_FLAG_RGB;
    const bool yuv = !rgb && !palette && desc->nb_components >= 3;
    const bool alpha = desc->flags & AV_PIX_FMT_FLAG_ALPHA;
    if(desc->nb_components < 1 || desc->nb_components > 4)
        throw std::runtime_error("Invalid image channels");
    ImagePixels image{frame.width, frame.height, {}};
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    std::array<std::vector<std::uint32_t>, 4> channels;
    std::array<float, 4> peaks{};
    for(unsigned c = 0; c < desc->nb_components; ++c) {
        const auto depth = desc->comp[c].depth;
        if(depth < 1 || depth > 16 || !frame.data[desc->comp[c].plane])
            throw std::runtime_error("Invalid image plane/depth");
        peaks[c] = static_cast<float>((1U << depth) - 1);
        channels[c].resize(image.width);
    }
    if(palette && !frame.data[1])
        throw std::runtime_error("Missing image palette");
    float kr = .299F, kb = .114F;
    if(frame.colorspace == AVCOL_SPC_BT709) {
        kr = .2126F;
        kb = .0722F;
    }
    if(frame.colorspace == AVCOL_SPC_BT2020_NCL) {
        kr = .2627F;
        kb = .0593F;
    }
    const float kg = 1 - kr - kb;
    for(int y = 0; y < image.height; ++y) {
        if(cancelled && cancelled())
            throw std::runtime_error("Image load cancelled");
        if(!palette)
            for(unsigned c = 0; c < desc->nb_components; ++c) {
                const bool chroma = yuv && (c == 1 || c == 2);
                const auto shift_x = chroma ? desc->log2_chroma_w : 0;
                const auto shift_y = chroma ? desc->log2_chroma_h : 0;
                const int width = (image.width + (1 << shift_x) - 1) >> shift_x;
                const std::uint8_t* planes[4]{frame.data[0], frame.data[1], frame.data[2], frame.data[3]};
                av_read_image_line2(channels[c].data(), planes, frame.linesize, desc, 0, y >> shift_y, c, width, 0, 4);
            }
        for(int x = 0; x < image.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 4;
            if(palette) {
                const auto index = frame.data[0][static_cast<std::ptrdiff_t>(y) * frame.linesize[0] + x];
                std::uint32_t color;
                std::memcpy(&color, frame.data[1] + index * 4, sizeof(color));
                image.rgba[offset] = static_cast<std::uint8_t>(color >> 16);
                image.rgba[offset + 1] = static_cast<std::uint8_t>(color >> 8);
                image.rgba[offset + 2] = static_cast<std::uint8_t>(color);
                image.rgba[offset + 3] = static_cast<std::uint8_t>(color >> 24);
                continue;
            }
            std::array<float, 3> color{};
            if(yuv) {
                float luma = channels[0][x] / peaks[0];
                const int chroma_x = x >> desc->log2_chroma_w;
                float cb = (static_cast<float>(channels[1][chroma_x]) - (peaks[1] + 1) / 2) / peaks[1];
                float cr = (static_cast<float>(channels[2][chroma_x]) - (peaks[2] + 1) / 2) / peaks[2];
                if(frame.color_range == AVCOL_RANGE_MPEG) {
                    luma = (luma - 16.0F / 255) * 255 / 219;
                    cb *= 255.0F / 224;
                    cr *= 255.0F / 224;
                }
                color = {luma + 2 * (1 - kr) * cr, luma - 2 * (1 - kb) * kb / kg * cb - 2 * (1 - kr) * kr / kg * cr, luma + 2 * (1 - kb) * cb};
            } else
                for(unsigned c = 0; c < 3; ++c) {
                    const auto source = rgb ? c : 0;
                    color[c] = channels[source][x] / peaks[source];
                    if(frame.format == AV_PIX_FMT_MONOWHITE)
                        color[c] = 1 - color[c];
                }
            for(unsigned c = 0; c < 3; ++c) image.rgba[offset + c] = static_cast<std::uint8_t>(std::round(std::clamp(color[c], 0.0F, 1.0F) * 255));
            image.rgba[offset + 3] = alpha ? static_cast<std::uint8_t>(std::round(channels[desc->nb_components - 1][x] / peaks[desc->nb_components - 1] * 255)) : 255;
        }
    }
    return image;
}
}  // namespace lumaframevr
