#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <span>

namespace media {
struct FrameData;
}

namespace lumaframevr {
struct FrameColor {
    int transfer{}, primaries{}, chroma_location{}, bits{};
    float mastering_min_nits{}, mastering_max_nits{}, max_cll{}, max_fall{};
    std::array<float, 8> mastering_xy{};
    std::array<float, 3> shader_controls() const;
};
class RetainedFrame {
   public:
    RetainedFrame();
    ~RetainedFrame();
    RetainedFrame(RetainedFrame&&) noexcept;
    RetainedFrame& operator=(RetainedFrame&&) noexcept;
    RetainedFrame(const RetainedFrame&) = delete;
    RetainedFrame& operator=(const RetainedFrame&) = delete;
    bool valid() const noexcept;
    bool software() const;
    int width() const;
    int height() const;
    int format() const;
    int colorspace() const;
    int color_range() const;
    float sample_aspect_ratio() const;
    double pts() const;
    FrameColor color() const;
    std::span<const unsigned char> plane_row(int plane, int row) const;
    const media::FrameData* native_frame() const;

   private:
    struct Storage;
    explicit RetainedFrame(std::unique_ptr<Storage> storage);
    std::unique_ptr<Storage> storage_;
    friend class DecoderSession;
};
}  // namespace lumaframevr
