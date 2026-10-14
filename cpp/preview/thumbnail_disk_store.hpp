#pragma once
#include "thumbnail_services.hpp"
#include "persistent_directory.hpp"
namespace lumaframevr {
class ThumbnailDiskStore {
   public:
    ThumbnailDiskStore(std::filesystem::path root, std::size_t budget);
    std::shared_ptr<ThumbnailPixels> load(const std::filesystem::path& source, unsigned width);
    bool save(const std::filesystem::path& source, unsigned width, const ThumbnailPixels& pixels, const PersistentPublish& publish = {});
    const std::string& error() const { return error_; }

   private:
    std::filesystem::path root_;
    std::unique_ptr<PersistentDirectory> directory_;
    bool writable_{true};
    std::size_t budget_, used_{};
    std::string error_, source_key_;
};
}  // namespace lumaframevr
