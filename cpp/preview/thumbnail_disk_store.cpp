#include "thumbnail_disk_store.hpp"
#include "thumbnail_source_identity.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace lumaframevr {
namespace {
constexpr auto identity = "LumaFrameVR thumbnail 1";
constexpr std::size_t maximum_pixels = 1024 * 1024 * 4;
std::uint64_t checksum(std::string_view bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for(const auto value : bytes) {
        hash ^= static_cast<unsigned char>(value);
        hash *= 1099511628211ULL;
    }
    return hash;
}
std::string timestamp(const std::filesystem::path& source) {
    auto ticks = std::filesystem::last_write_time(source).time_since_epoch().count();
    const bool negative = ticks < 0;
    std::string result;
    do {
        const auto digit = ticks % 10;
        result.push_back(static_cast<char>('0' + (digit < 0 ? -digit : digit)));
        ticks /= 10;
    } while(ticks);
    if(negative)
        result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
}
std::string fingerprint(const std::filesystem::path& source, unsigned width) {
    const auto identity = thumbnail_source_identity(source);
    return std::filesystem::canonical(source).string() + '\n' + std::to_string(std::filesystem::file_size(source)) + '\n' + timestamp(source) + '\n' + thumbnail_identity_key(identity) + '\n' +
           std::to_string(width);
}
std::filesystem::path record_path(const std::filesystem::path& root, std::string_view key) {
    std::ostringstream name;
    name << std::hex << checksum(key) << ".lft";
    return root / name.str();
}
}  // namespace
ThumbnailDiskStore::ThumbnailDiskStore(std::filesystem::path root, std::size_t budget) : root_(std::move(root)), budget_(budget) {
    if(budget_ < std::string_view(identity).size())
        throw std::runtime_error("Thumbnail cache is full; using memory thumbnails");
    directory_ = std::make_unique<PersistentDirectory>(root_, true);
    directory_->lock_writer();
    used_ = directory_->retained_bytes(budget_);
}
std::shared_ptr<ThumbnailPixels> ThumbnailDiskStore::load(const std::filesystem::path& source, unsigned width) {
    try {
        const auto key = fingerprint(source, width);
        source_key_ = key;
        const auto bytes = directory_->read(record_path(root_, key).filename().string(), maximum_pixels + 16384);
        if(!bytes)
            return {};
        std::istringstream input(*bytes);
        std::string header, actual;
        std::uint64_t expected{};
        int w{}, h{};
        std::getline(input, header);
        if(header != identity || !(input >> std::quoted(actual) >> w >> h >> expected) || actual != key || w <= 0 || h <= 0 || w > 1024 || h > 1024 || input.get() != '\n')
            throw std::runtime_error("Invalid cached thumbnail header");
        const auto offset = input.tellg();
        const auto size = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
        if(offset < 0 || static_cast<std::size_t>(offset) > bytes->size() || bytes->size() - static_cast<std::size_t>(offset) != size)
            throw std::runtime_error("Truncated or trailing cached thumbnail data");
        const auto pixels = std::string_view(*bytes).substr(static_cast<std::size_t>(offset));
        if(checksum(pixels) != expected)
            throw std::runtime_error("Cached thumbnail checksum mismatch");
        if(fingerprint(source, width) != key)
            throw std::runtime_error("Thumbnail source changed during cache read");
        return std::make_shared<ThumbnailPixels>(ThumbnailPixels{{pixels.begin(), pixels.end()}, w, h});
    } catch(const std::exception& error) {
        error_ = error.what();
        writable_ = false;
        return {};
    }
}
bool ThumbnailDiskStore::save(const std::filesystem::path& source, unsigned width, const ThumbnailPixels& pixels, const PersistentPublish& publish) {
    if(!writable_)
        return false;
    try {
        if(pixels.width <= 0 || pixels.height <= 0 || pixels.width > 1024 || pixels.height > 1024 ||
           pixels.pixels.size() != static_cast<std::size_t>(pixels.width) * static_cast<std::size_t>(pixels.height) * 4)
            throw std::runtime_error("Thumbnail exceeds persistent pixel bound");
        const auto key = fingerprint(source, width);
        if(key != source_key_)
            throw std::runtime_error("Thumbnail source changed during decode; cache write skipped");
        const auto file = record_path(root_, key);

        const std::string_view data(reinterpret_cast<const char*>(pixels.pixels.data()), pixels.pixels.size());
        std::ostringstream header;
        header << identity << '\n' << std::quoted(key) << ' ' << pixels.width << ' ' << pixels.height << ' ' << checksum(data) << '\n';
        auto bytes = header.str();
        bytes.append(data);
        used_ = std::max(used_, directory_->retained_bytes(budget_));
        if(bytes.size() > budget_ || used_ > budget_ - bytes.size())
            throw std::runtime_error("Thumbnail cache is full; using memory thumbnails");
        used_ += bytes.size();
        directory_->write(file.filename().string(), bytes, {}, publish);
        return true;
    } catch(const std::exception& error) {
        error_ = error.what();
        writable_ = false;
        return false;
    }
}
}  // namespace lumaframevr
