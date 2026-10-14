#include "decoder_internal.hpp"
#include <algorithm>
#include <cmath>

namespace lumaframevr {
RetainedFrame::RetainedFrame() = default;
RetainedFrame::RetainedFrame(std::unique_ptr<Storage> storage) : storage_(std::move(storage)) {}
RetainedFrame::~RetainedFrame() = default;
RetainedFrame::RetainedFrame(RetainedFrame&&) noexcept = default;
RetainedFrame& RetainedFrame::operator=(RetainedFrame&&) noexcept = default;
bool RetainedFrame::valid() const noexcept {
    return static_cast<bool>(storage_);
}
const media::FrameData* RetainedFrame::native_frame() const {
    if(!storage_)
        throw std::logic_error("Empty decoded frame");
    return &storage_->pixels.get();
}
bool RetainedFrame::software() const {
    return native_frame()->software != 0;
}
int RetainedFrame::width() const {
    return native_frame()->width;
}
int RetainedFrame::height() const {
    return native_frame()->height;
}
int RetainedFrame::format() const {
    return native_frame()->format;
}
int RetainedFrame::colorspace() const {
    return native_frame()->colorspace;
}
int RetainedFrame::color_range() const {
    return native_frame()->full_range;
}
float RetainedFrame::sample_aspect_ratio() const {
    return native_frame()->sample_aspect_ratio;
}
double RetainedFrame::pts() const {
    return native_frame()->pts;
}
FrameColor RetainedFrame::color() const {
    const auto& c = native_frame()->color;
    FrameColor result{c.transfer, c.primaries, c.chroma_location, c.bits, c.mastering_min_nits, c.mastering_max_nits, c.max_cll, c.max_fall, {}};
    std::copy(std::begin(c.mastering_xy), std::end(c.mastering_xy), result.mastering_xy.begin());
    return result;
}
std::span<const unsigned char> RetainedFrame::plane_row(int plane, int row) const {
    const auto& pixels = *native_frame();
    if(!pixels.software)
        throw std::logic_error("DMA-BUF has no CPU plane");
    if(plane < 0 || plane >= pixels.planes || plane >= 3)
        throw std::out_of_range("Frame plane");
    const auto rows = plane == 0 ? pixels.height : (pixels.height + 1) / 2;
    if(row < 0 || row >= rows || !pixels.data[plane])
        throw std::out_of_range("Frame row");
    const auto samples = plane == 0 ? pixels.width : (pixels.width + 1) / 2;
    const auto wide = pixels.format == AV_PIX_FMT_YUV420P10LE || pixels.format == AV_PIX_FMT_P010LE;
    const auto interleaved = plane == 1 && (pixels.format == AV_PIX_FMT_NV12 || pixels.format == AV_PIX_FMT_P010LE);
    const auto bytes = samples * (wide ? 2 : 1) * (interleaved ? 2 : 1);
    if(std::abs(pixels.linesize[plane]) < bytes)
        throw std::runtime_error("Decoded plane stride too short");
    return {pixels.data[plane] + static_cast<std::ptrdiff_t>(row) * pixels.linesize[plane], static_cast<std::size_t>(bytes)};
}
std::array<float, 3> FrameColor::shader_controls() const {
    float peak = 1000;
    for(float value : {max_cll, mastering_max_nits}) {
        if(std::isfinite(value) && value >= 100) {
            peak = value;
            break;
        }
    }
    return {static_cast<float>(transfer), static_cast<float>(primaries), std::clamp(peak, 100.0F, 10000.0F)};
}
}  // namespace lumaframevr
