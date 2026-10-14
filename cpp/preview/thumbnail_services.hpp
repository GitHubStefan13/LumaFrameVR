#pragma once
#include "decoder_session.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace lumaframevr {
struct ThumbnailRequest {
    std::filesystem::path path;
    unsigned position_percent{10};
    std::optional<double> seconds;
    bool operator==(const ThumbnailRequest&) const = default;
};
struct DecodedPreview {
    RetainedFrame frame;
    int width{}, height{};
    std::optional<double> duration;
    DecoderDescription decoder;
};
DecodedPreview decode_preview(const ThumbnailRequest& request);
struct ThumbnailPixels {
    std::vector<unsigned char> pixels;
    int width{}, height{};
};
ThumbnailPixels thumbnail_pixels(const RetainedFrame& frame, unsigned target);
struct PreviewResult {
    ThumbnailRequest request;
    std::shared_ptr<DecodedPreview> preview;
    std::string error;
    std::uint64_t generation{};
};
class PreviewFetcher {
   public:
    PreviewFetcher();
    ~PreviewFetcher();
    PreviewFetcher(const PreviewFetcher&) = delete;
    PreviewFetcher& operator=(const PreviewFetcher&) = delete;
    std::uint64_t request(ThumbnailRequest request);
    void clear();
    std::optional<PreviewResult> poll();
    std::optional<ThumbnailRequest> pending() const;

   private:
    struct State;
    std::unique_ptr<State> state_;
};
struct TileWanted {
    std::vector<std::pair<std::size_t, std::filesystem::path>> entries;
    unsigned width{};
    bool operator==(const TileWanted&) const = default;
};
struct TileShown {
    std::size_t index;
    std::shared_ptr<const ThumbnailPixels> tile;
};
class ThumbnailCache {
   public:
    ThumbnailCache();
    ~ThumbnailCache();
    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;
    void want(const TileWanted& wanted);
    void update();
    std::vector<TileShown> shown(const TileWanted& wanted) const;
    void reset();
    void persistence(std::optional<std::filesystem::path> directory, std::size_t budget = 128 * 1024 * 1024);
    std::uint64_t disk_hits() const;
    std::string persistence_error() const;
    std::size_t held() const;
    std::size_t failed() const;
    std::optional<std::string> failure(const std::filesystem::path& path, unsigned width) const;
    std::size_t bytes() const;
    std::size_t sources() const;
    std::uint64_t revision() const;

   private:
    struct State;
    std::unique_ptr<State> state_;
};
class PreviewCadence {
   public:
    explicit PreviewCadence(std::optional<double> previous = std::nullopt) : previous_(previous) {}
    bool due(double now);
    void reset() { previous_.reset(); }

   private:
    std::optional<double> previous_;
};
class SeekPreviewReadout {
   public:
    bool arm(std::uint64_t request);
    bool should_paint(bool frame) const;
    void painted(double pts);
    void visibility(bool transport_visible);
    std::optional<double> shown() const { return shown_; }
    static float center(double position, std::optional<double> duration);

   private:
    std::optional<std::uint64_t> request_;
    std::optional<double> shown_;
    bool armed_{};
};
}  // namespace lumaframevr
