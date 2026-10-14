#pragma once
#include "video_presentation.hpp"
#include <filesystem>
#include <optional>

namespace lumaframevr {
template <class T>
struct Hint {
    std::optional<T> value;
    bool conflict{};
    Hint() = default;
    Hint(T known) : value(known) {}
    static Hint conflicting() {
        Hint result;
        result.conflict = true;
        return result;
    }
    Hint combine(const Hint& other) const {
        if(conflict || other.conflict)
            return conflicting();
        if(value && other.value && value != other.value)
            return conflicting();
        return value ? *this : other;
    }
    T apply(T fallback) const { return !conflict && value ? *value : fallback; }
};
struct ProjectionHint {
    Projection projection;
    unsigned degrees{180};
    bool operator==(const ProjectionHint&) const = default;
};
struct VideoHints {
    Hint<Projection> projection;
    Hint<unsigned> fisheye_degrees;
    Hint<Stereo> stereo;
    Hint<StereoFormat> sbs_format, tb_format;
    Hint<bool> swap_eyes;
    static VideoHints filename(const std::filesystem::path& path);
    static VideoHints native(const std::array<int, 3>& fields);
    VideoHints combine(const VideoHints& other) const;
    VideoPresentation apply(VideoPresentation fallback) const;
    bool immersive() const;
};
}  // namespace lumaframevr
