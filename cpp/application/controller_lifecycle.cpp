#include "application_controller.hpp"
#include "media_file_type.hpp"
#include <iostream>
namespace lumaframevr::application {
namespace {
std::filesystem::path initial_directory(const ApplicationOptions& options) {
    if(options.restore_last_folder && options.viewer_state) {
        const auto saved = ViewerPreferenceStore(options.viewer_state).load();
        std::error_code error;
        if(saved.remember_last_state && std::filesystem::is_directory(saved.folder, error))
            return saved.folder;
    }
    if(options.source && options.source->remote())
        return std::filesystem::current_path();
    return std::filesystem::is_regular_file(options.file) ? options.file.parent_path() : options.file;
}
}  // namespace
Controller::Controller(const ApplicationOptions& options)
    : explorer(initial_directory(options)), presentation(options.presentation), path(options.file), hud(options.hud), viewer_store_(options.viewer_state), settings_(options.overrides) {
    initialize_viewer(options);
    presentation = settings_.open(path);
    advanced_ = std::make_unique<ui::AdvancedSettingsController>(settings_, path, presentation);
    if(!(options.source && options.source->remote()) && std::filesystem::is_regular_file(path) && !supported_media_file(path)) {
        error = i18n::tr("error.unsupported");
        return;
    }
    if((options.source && options.source->remote()) || std::filesystem::is_regular_file(path)) {
        if(!(options.source && options.source->remote()) && is_image_path(path)) {
            images_.open(path);
            settings_.infer(path, {}, presentation);
        } else
            playback_.start(options.input(), std::pair{volume_, muted_});
        telemetry.remote = options.input().remote();
        apply_volume();
    }
}
Controller::~Controller() {
    persist_viewer();
}
void Controller::start_pending() {
    changed |= playback_.reap();
    const auto pending = playback_.take_pending();
    if(!pending)
        return;
    path = *pending;
    presentation = settings_.open(path);
    advanced_ = std::make_unique<ui::AdvancedSettingsController>(settings_, path, presentation);
    if(is_image_path(path)) {
        images_.open(path);
        settings_.infer(path, {}, presentation);
    } else
        playback_.start(MediaSource::parse(path.string()), std::pair{volume_, muted_});
    telemetry = {};
    paused_ = false;
    volume_pending_ = true;
    apply_volume();
    if(playback_.current())
        playback_.current()->suspend(suspended_);
    changed = true;
}
void Controller::stop() {
    playback_.stop();
    images_.stop();
    viewer_settings_open_ = false;
    dialogs.close();
    volume_open = false;
    visible_until_ = 0;
    seek_generation_.reset();
    seek_repeat_ = {};
    list_drag_.reset();
    panel_drag_.reset();
    alignment_drag_.reset();
    settings_drag_.reset();
    changed = true;
}
void Controller::synchronize(const std::function<void()>& release_graphics) {
    if(images_.release_required()) {
        release_graphics();
        images_.released();
    }
    if(!images_.active()) {
        bool failed = false;
        playback_.synchronize(release_graphics, [&](const std::exception& error) {
            fail(error.what());
            failed = true;
        });
        if(failed)
            playback_.synchronize(release_graphics);
    }
}
void Controller::fail(const std::string& message) {
    error = message;
    std::cerr << "Playback failed: " << error << '\n';
    stop();
}
void Controller::select_entry(std::size_t index) {
    if(!explorer.browser.select(index) || retiring())
        return;
    const auto file = explorer.browser.open();
    if(file && !supported_media_file(*file)) {
        error = i18n::tr("error.unsupported");
        changed = true;
        ++revision;
        return;
    }
    if(file) {
        stop();
        playback_.queue(*file);
        error.clear();
    }
    changed = true;
}
RetainedFrame Controller::frame() {
    if(!playback_.current())
        return {};
    try {
        if(volume_pending_)
            apply_volume();
        if(!playback_.current())
            return {};
        auto result = playback_.current()->poll();
        const auto status = playback_.current()->snapshot();
        settings_.infer(path, VideoHints::native(status.metadata), presentation);
        telemetry.decoder = status.decoder;
        telemetry.bitrate_mbps = status.decoder.bitrate_mbps;
        telemetry.sync.video_queue = status.queue_depth;
        telemetry.stream.buffered_bytes = status.buffer.bytes;
        telemetry.stream.buffer_ms = static_cast<double>(status.buffer.video_us) / 1000;
        telemetry.stream.filling = !status.ready;
        if(result.valid()) {
            telemetry.pts = result.pts();
            if(status.audio.clock)
                telemetry.sync.observe((result.pts() - *status.audio.clock) * 1000, telemetry.sync.dropped);
            seek_generation_.reset();
            ++revision;
        } else if(playback_.current()->finished() && status.audio.done)
            stop();
        return result;
    } catch(const std::exception& failure) {
        fail(failure.what());
        return {};
    }
}
RetainedFrame Controller::take_preview() {
    if(playback_.current())
        return playback_.current()->take_preview();
    return explorer.take_preview();
}
void Controller::uploaded() {
    settings_.uploaded(path, presentation);
}
void Controller::suspend(bool value) {
    suspended_ = value;
    if(playback_.current())
        playback_.current()->suspend(value);
    if(value) {
        seek_repeat_ = {};
        triggers_ = {};
        hover = {};
    }
}
WorkerSnapshot Controller::snapshot() const {
    return playback_.current() ? playback_.current()->snapshot() : WorkerSnapshot{};
}
void Controller::apply_volume() {
    persist_viewer();
    if(!playback_.current())
        return;
    try {
        const auto status = playback_.current()->snapshot();
        if(status.buffer.audio_us > 0 || status.audio.clock) {
            playback_.current()->volume(volume_, muted_);
            volume_pending_ = false;
            std::cerr << "Audio volume queued: " << volume_ << "; muted=" << muted_ << '\n';
        } else if(status.audio.done)
            volume_pending_ = false;
    } catch(const std::exception& error) {
        fail(error.what());
    }
}
}  // namespace lumaframevr::application
