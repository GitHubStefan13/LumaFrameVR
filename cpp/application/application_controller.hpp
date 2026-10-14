#pragma once
#include "application_options.hpp"
#include "controller_state.hpp"
#include "playback_timeline.hpp"
#include "advanced_dialog_state.hpp"
#include "advanced_panels.hpp"
#include "thumbnail_services.hpp"
#include "browser.hpp"
#include "transport.hpp"
#include "seek_repeat.hpp"
#include "telemetry.hpp"
#include <functional>
#include "playback_lifetime.hpp"
#include "browser_previews.hpp"
#include "scene_snapshot.hpp"
#include "image_viewing.hpp"
#include "viewer_preferences.hpp"
#include "viewer_settings.hpp"
#include "update_process.hpp"
namespace lumaframevr::application {
class Controller {
   public:
    explicit Controller(const ApplicationOptions& options);
    ~Controller();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    void update(const ControllerState& controls, const std::array<std::optional<Hit>, 2>& hover, double now);
    std::optional<PanelKind> grabbed_panel(unsigned hand) const;
    void pointer(unsigned hand, std::optional<Hit> hit, bool held, double now);
    void activate(const Hit& hit, double now);
    void synchronize(const std::function<void()>& release_graphics);
    void stop();
    void fail(const std::string& message);
    void select_entry(std::size_t index);
    void seek(double seconds, double now);
    void transport_button(unsigned button, double now);
    void toggle_passthrough();
    void browser_action(const ui::BrowserTarget& target);
    RetainedFrame frame();
    RetainedFrame take_preview();
    void uploaded();
    void suspend(bool suspended);
    void update_previews();
    TileWanted wanted_tiles() const;
    const TileWanted& visible_tiles() const { return explorer.visible(); }
    ui::TransportState transport() const;
    std::array<std::optional<PanelKind>, 3> panels() const;
    bool transport_visible(double now) const;
    bool startup_visible(double now) const { return startup_started_ && now < *startup_started_ + 2; }
    bool playing() const { return playback_.playing() || images_.active(); }
    bool viewing_image() const { return images_.active(); }
    bool image_loading() const { return images_.loading(); }
    std::array<int, 2> image_dimensions() const { return images_.dimensions(); }
    std::optional<ImagePixels> take_image() { return images_.take(); }
    bool retiring() const { return playback_.retiring(); }
    const std::optional<std::filesystem::path>& queued() const { return playback_.queued(); }
    std::optional<std::uint64_t> seek_generation() const { return seek_generation_; }
    std::uint64_t preview_requests() const { return preview_requests_; }
    WorkerSnapshot snapshot() const;
    SceneSnapshot scene_snapshot(double now) const;
    PanelSnapshot panel_snapshot() const;
    ui::AdvancedSettingsController& advanced() { return *advanced_; }
    ui::ViewerSettingsState viewer_settings() const;
    bool language_prompt() const { return language_prompt_; }
    // Set by the Quit control in the file viewer. The frame loop turns it into the same
    // clean shutdown a signal or a closed window starts.
    void request_exit() { exit_requested_ = true; }
    bool exit_requested() const { return exit_requested_; }
    void begin_first_start();
    bool pending_volume() const { return volume_pending_; }
    void set_viewer_setting(std::size_t index, bool enabled);
    BrowserPreviews explorer;
    VideoPresentation presentation;
    std::filesystem::path path;
    ui::AdvancedDialogs dialogs;
    diagnostics::Telemetry telemetry;
    diagnostics::DeviceSample device_sample;
    double xr_fps{}, target_fps{};
    std::array<std::optional<Hit>, 2> hover;
    std::array<float, 2> bar_offset{};
    std::string error;
    bool passthrough_enabled{}, passthrough_supported{};
    std::string passthrough_reason{"Camera passthrough capability has not been reported by the XR runtime"};
    bool hud{}, volume_open{}, changed{true};
    std::uint64_t revision{};

   private:
    std::string passthrough_error_;
    ViewerPreferenceStore viewer_store_;
    ViewerPreferences viewer_preferences_;
    std::optional<std::filesystem::path> viewer_state_path_;
    UpdateProcess updater_;
    std::size_t update_page_{};
    void start_viewer_update();
    bool viewer_settings_open_{};
    bool exit_requested_{};
    std::size_t viewer_setting_selected_{};
    bool language_prompt_{};
    std::size_t language_selected_{};
    int language_direction_{};
    void initialize_viewer(const ApplicationOptions& options);
    void persist_viewer();
    void click_viewer_setting(const Hit& hit);
    void click_language_prompt(const Hit& hit);
    void set_viewer_language(std::string_view code);
    void confirm_language();
    bool language_input(const ControllerState& controls);
    void navigate(const ControllerState& controls, double now);
    void start_pending();
    template <class Seek>
    void sought_from(Seek seek, double now, bool preview) {
        try {
            sought(seek(), now, preview);
        } catch(const std::exception& error) {
            fail(error.what());
        }
    }
    void adjacent_image(int direction);
    void apply_volume();
    void sought(std::optional<std::uint64_t> generation, double now, bool preview);
    bool interacted(const ControllerState& controls) const;
    void drag_list(const Hit& hit);
    RememberedVideoSettings settings_;
    std::unique_ptr<ui::AdvancedSettingsController> advanced_;
    PlaybackLifetime playback_;
    ImageViewing images_;
    std::optional<std::uint64_t> seek_generation_;
    std::array<bool, 2> grips_{}, triggers_{};
    std::array<SeekRepeat, 2> seek_repeat_;
    struct Drag {
        unsigned hand;
        Hit start;
        int last_y{}, carry{};
        std::size_t entry{};
        bool scrolled{};
    };
    std::optional<Drag> list_drag_, panel_drag_;
    std::optional<std::pair<unsigned, std::size_t>> alignment_drag_;
    struct SettingsDrag {
        unsigned hand;
        ui::SettingField field;
        ui::Rect slider;
    };
    std::optional<SettingsDrag> settings_drag_;
    float volume_{0.8F};
    bool volume_pending_{true};
    bool muted_{}, paused_{}, suspended_{}, minimized_{};
    double visible_until_{}, next_navigation_{};
    PreviewCadence preview_cadence_{0.0};
    std::uint64_t preview_requests_{};
    std::optional<double> startup_started_;
    std::array<int, 2> last_direction_{};
};
}  // namespace lumaframevr::application
