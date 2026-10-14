#pragma once
#include "video_hints.hpp"
#include <map>

namespace lumaframevr {
class RememberedVideoSettings {
   public:
    explicit RememberedVideoSettings(VideoHints overrides = {});
    VideoPresentation open(const std::filesystem::path& path);
    void infer(const std::filesystem::path& path, const VideoHints& metadata, VideoPresentation& current);
    void uploaded(const std::filesystem::path& path, const VideoPresentation& current);
    void edit(const std::filesystem::path& path, const VideoPresentation& current);
    void remember(const std::filesystem::path& path, const VideoPresentation& current);
    bool needs_inference() const { return !resolved_; }

   private:
    std::map<std::filesystem::path, VideoPresentation> remembered_;
    VideoPresentation last_;
    VideoHints overrides_;
    bool resolved_{}, played_{};
};
}  // namespace lumaframevr
