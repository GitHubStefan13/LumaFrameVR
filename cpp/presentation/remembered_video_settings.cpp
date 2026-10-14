#include "remembered_video_settings.hpp"

namespace lumaframevr {
RememberedVideoSettings::RememberedVideoSettings(VideoHints overrides) : last_(overrides.apply({})), overrides_(std::move(overrides)) {}
VideoPresentation RememberedVideoSettings::open(const std::filesystem::path& path) {
    played_ = false;
    const auto found = remembered_.find(path);
    resolved_ = found != remembered_.end();
    return resolved_ ? found->second : overrides_.apply(last_);
}
void RememberedVideoSettings::infer(const std::filesystem::path& path, const VideoHints& metadata, VideoPresentation& current) {
    if(resolved_)
        return;
    const auto hints = metadata.combine(VideoHints::filename(path));
    auto fallback = hints.immersive() ? last_ : VideoPresentation::flat();
    fallback.alignment = current.alignment;
    current = overrides_.apply(hints.apply(fallback));
    resolved_ = true;
}
void RememberedVideoSettings::uploaded(const std::filesystem::path& path, const VideoPresentation& current) {
    if(played_)
        return;
    played_ = true;
    edit(path, current);
}
void RememberedVideoSettings::edit(const std::filesystem::path& path, const VideoPresentation& current) {
    resolved_ = true;
    remember(path, current);
}
void RememberedVideoSettings::remember(const std::filesystem::path& path, const VideoPresentation& current) {
    if(!played_)
        return;
    remembered_[path] = current;
    last_ = current;
    last_.alignment = {};
    last_.lens = {};
}
}  // namespace lumaframevr
