#include "thumbnail_cache_state.hpp"
#include "image_thumbnail.hpp"
#include "media_decoder_startup.hpp"
#include "thumbnail_disk_store.hpp"
#include "thumbnail_source_identity.hpp"
#include <algorithm>
namespace lumaframevr {
std::size_t ThumbnailCache::State::bytes() const {
    std::size_t count = 0;
    for(const auto& [key, entry] : ready) {
        static_cast<void>(key);
        count += entry.pixels->pixels.size();
    }
    return count;
}
void ThumbnailCache::State::evict() {
    auto count = bytes();
    while(ready.size() > 64 || count > 8 * 1024 * 1024) {
        const auto oldest = std::min_element(ready.begin(), ready.end(), [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
        count -= oldest->second.pixels->pixels.size();
        ready.erase(oldest);
    }
    while(failures.size() > 64) failures.erase(failures.begin());
}
void ThumbnailCache::State::run() {
    const media::PreviewDecode preview_decode;
    struct Source {
        std::unique_ptr<DecoderSession> decoder;
        ThumbnailSourceIdentity identity;
        std::uint64_t used;
    };
    std::map<std::filesystem::path, Source> decoders;
    std::uint64_t clock{}, disk_serial{};
    std::unique_ptr<ThumbnailDiskStore> disk;
    for(;;) {
        Job job;
        std::optional<std::filesystem::path> disk_directory;
        std::size_t disk_budget{};
        std::uint64_t requested_disk_serial{};
        std::shared_ptr<ThumbnailPersistenceOwner> disk_owner;
        bool has_job{}, clear_decoders{};
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return stopped || reset_worker || disk_serial != this->disk_serial || !pending.empty(); });
            if(stopped)
                return;
            if(reset_worker) {
                reset_worker = false;
                clear_decoders = true;
            }
            disk_directory = this->disk_directory;
            disk_budget = this->disk_budget;
            requested_disk_serial = this->disk_serial;
            disk_owner = this->disk_owner;
            if(!pending.empty()) {
                job = std::move(pending.back());
                pending.pop_back();
                has_job = true;
            }
        }
        if(clear_decoders) {
            decoders.clear();
            source_count = 0;
        }
        std::string disk_error;
        if(requested_disk_serial != disk_serial) {
            disk.reset();
            disk_serial = requested_disk_serial;
            if(disk_directory)
                try {
                    disk = std::make_unique<ThumbnailDiskStore>(*disk_directory, disk_budget);
                } catch(const std::exception& error) {
                    disk_error = error.what();
                }
        }
        if(!has_job) {
            std::lock_guard lock(mutex);
            if(!disk_error.empty() && disk_serial == this->disk_serial)
                this->disk_error = std::move(disk_error);
            continue;
        }
        Completed result{job, {}, {}};
        bool disk_hit = false;
        try {
            const auto identity = thumbnail_source_identity(job.key.first);
            if(disk) {
                result.pixels = disk->load(job.key.first, job.key.second);
                disk_hit = static_cast<bool>(result.pixels);
            }
            if(!result.pixels && is_image_path(job.key.first)) {
                const auto cancelled = [this, serial = job.reset] {
                    std::lock_guard lock(mutex);
                    return stopped || serial != reset_serial;
                };
                auto pixels = thumbnail_image(read_image(job.key.first, cancelled), job.key.second);
                result.pixels = std::make_shared<ThumbnailPixels>(ThumbnailPixels{std::move(pixels.rgba), pixels.width, pixels.height});
            } else if(!result.pixels) {
                auto source = decoders.find(job.key.first);
                if(source != decoders.end() && source->second.identity != identity) {
                    decoders.erase(source);
                    source = decoders.end();
                }
                if(source == decoders.end()) {
                    if(decoders.size() == 4) {
                        const auto oldest = std::min_element(decoders.begin(), decoders.end(), [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
                        decoders.erase(oldest);
                    }
                    auto decoder = std::make_unique<DecoderSession>(MediaSource::parse(job.key.first.string()), true);
                    if(thumbnail_source_identity(job.key.first) != identity)
                        throw std::runtime_error("Thumbnail source changed while opening decoder");
                    source = decoders.emplace(job.key.first, Source{std::move(decoder), identity, ++clock}).first;
                }
                source->second.used = ++clock;
                auto& decoder = *source->second.decoder;
                if(const auto duration = decoder.duration())
                    decoder.seek(*duration * 0.1);
                auto frame = decoder.next_frame();
                if(!frame.valid() || !frame.software())
                    throw std::runtime_error("No software thumbnail frame");
                result.pixels = std::make_shared<ThumbnailPixels>(thumbnail_pixels(frame, job.key.second));
            }
            if(thumbnail_source_identity(job.key.first) != identity)
                throw std::runtime_error("Thumbnail source changed during decode");
        } catch(const std::exception& error) {
            result.pixels.reset();
            result.error = error.what();
        }
        if(disk && result.pixels && !disk_hit) {
            const PersistentPublish publish = [disk_owner](const std::function<void()>& commit) { return disk_owner->publish(commit); };
            disk->save(job.key.first, job.key.second, *result.pixels, publish);
        }
        if(disk)
            disk_error = disk->error();
        {
            std::lock_guard lock(mutex);
            if(stopped)
                return;
            if(disk_hit && disk_serial == this->disk_serial && job.reset == reset_serial)
                ++disk_hits;
            if(!disk_error.empty() && disk_serial == this->disk_serial)
                this->disk_error = std::move(disk_error);
            source_count = job.reset == reset_serial && !reset_worker ? decoders.size() : 0;
            if(completed.size() == 64)
                completed.pop_front();
            completed.push_back(std::move(result));
        }
    }
}
}  // namespace lumaframevr
