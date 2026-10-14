#include "application_run.hpp"
#include "openxr_input.hpp"
#include "application_renderer.hpp"
#include "application_panels.hpp"
#include "gpu_preview.hpp"
#include "controller_layer.hpp"
#include "application_passthrough.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace lumaframevr::application::model_detail {
// Declared here rather than including model_openvr.hpp, which needs the vendored
// OpenVR headers that this translation unit does not otherwise use.
void shutdown_sessions();
}  // namespace lumaframevr::application::model_detail
namespace lumaframevr::application {
namespace {
volatile std::sig_atomic_t interrupted{};
// Leaving at once. Used by the second signal and by the watchdog below.
void force_exit(int) {
    _exit(130);
}
// The first signal asks the frame loop to stop. That is not enough on its own:
// xrWaitFrame can sit inside the runtime for seconds, and joining the controller
// model worker can wait on a blocking OpenVR call, so the signal would be noticed
// far too late and a desktop quit or Steam's Exit Game would look like it did
// nothing. The alarm gives the clean shutdown four seconds, then ends the process.
void interrupt(int) {
    if(interrupted)
        _exit(130);
    interrupted = 1;
    alarm(4);
}
std::optional<Pose> head_pose(OpenXrSession& session, XrTime time) {
    XrSpaceLocation location{};
    location.type = XR_TYPE_SPACE_LOCATION;
    xr_check(xrLocateSpace(session.view_space(), session.local_space(), time, &location), "locate head in controller local space");
    const auto valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    if((location.locationFlags & valid) != valid)
        return {};
    return pose_of(location.pose);
}
}  // namespace
int run(const ApplicationOptions& options) {
    interrupted = 0;
    std::signal(SIGINT, interrupt);
    std::signal(SIGTERM, interrupt);
    std::signal(SIGHUP, interrupt);
    std::signal(SIGALRM, force_exit);
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); };
    auto runtime = OpenXrRuntime::create();
    OpenXrSession session(runtime);
    std::cerr << "OpenXR: " << runtime->name() << "; Vulkan: " << session.graphics()->name() << '\n';
    OpenXrInput input(session);
    Controller app(options);
    app.begin_first_start();
    OpenXrPassthrough camera(session);
    synchronize_passthrough(app, camera, false, session.passthrough_fault());
    std::cerr << "Passthrough: " << camera.detail() << "; default=off\n";
    const auto shaders = shader_directory();
    Renderer renderer(session, shaders);
    const auto packaged_openvr = shaders.parent_path() / "runtime/libopenvr_api.so";
    RuntimeControllerModels controller_models(std::filesystem::is_regular_file(packaged_openvr) ? packaged_openvr.string() : "libopenvr_api.so");
    std::array<std::string, 2> model_diagnostics;
    ControllerLayer controllers(session, shaders);
    Panels panels(session);
    GpuPreview preview(session, shaders);
    Scene scene;
    auto hud_snapshot = options.hud_snapshot, video_snapshot = options.snapshot;
    std::uint64_t frames{}, measured_frames{};
    double measured_at{};
    XrTime previous_display{};
    diagnostics::DeviceMetrics metrics;
    // Quit from the viewer now goes through the runtime, exactly like the overlay's Exit
    // Game: the session is asked to end and the loop runs until the runtime confirms. A
    // runtime that never answers must not hold the process open, so the request carries a
    // deadline of its own.
    const auto exit_grace = std::chrono::milliseconds(750);
    auto exit_deadline = std::chrono::steady_clock::time_point{};
    while(!interrupted && (!options.seconds || elapsed() < *options.seconds) && session.poll()) {
        const auto loop_now = std::chrono::steady_clock::now();
        if(app.exit_requested() && !session.exit_requested()) {
            std::cerr << "XR: viewer asked to quit; requesting session exit from the runtime\n";
            session.request_exit();
            exit_deadline = loop_now + exit_grace;
        } else if(app.exit_requested() && loop_now > exit_deadline) {
            std::cerr << "XR: the runtime did not end the session; shutting down directly\n";
            break;
        }
        if(session.exiting())
            break;
        if(!session.running()) {
            synchronize_passthrough(app, camera, false, session.passthrough_fault());
            app.suspend(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        auto frame = session.begin_frame();
        app.suspend(!frame->renderable());
        if(!frame->renderable()) {
            synchronize_passthrough(app, camera, false, session.passthrough_fault());
            frame->finish({}, false);
            continue;
        }
        const auto hands = session.focused() ? input.poll(frame->display_time()) : ControllerState{};
        const auto head = head_pose(session, frame->display_time());
        const auto now = elapsed();
        if(previous_display && frame->display_time() > previous_display)
            app.target_fps = 1e9 / static_cast<double>(frame->display_time() - previous_display);
        previous_display = frame->display_time();
        if(now - measured_at >= .5) {
            app.xr_fps = static_cast<double>(frames - measured_frames) / (now - measured_at);
            measured_frames = frames;
            measured_at = now;
        }
        if(app.hud && metrics.refresh()) {
            app.device_sample = metrics.latest;
            app.changed = true;
        }
        scene.update(app.scene_snapshot(now), hands, head, now);
        const std::array<std::optional<Hit>, 2> hits{scene.pick(0), scene.pick(1)};
        app.update(hands, hits, now);
        for(unsigned hand = 0; hand < 2; ++hand) {
            auto hit = hits[hand];
            if(const auto grabbed = app.grabbed_panel(hand))
                hit = scene.drag(hand, *grabbed);
            app.pointer(hand, hit, hands.triggers[hand] > .5F, now);
        }
        app.synchronize([&] {
            renderer.clear();
            if(app.playing() || app.retiring())
                preview.clear();
        });
        auto picture = app.frame();
        if(picture.valid()) {
            try {
                renderer.upload(std::move(picture));
                app.uploaded();
            } catch(const std::exception& error) {
                renderer.clear();
                app.fail(error.what());
            }
        }
        if(auto image_pixels = app.take_image()) {
            try {
                renderer.upload(std::move(*image_pixels));
                app.uploaded();
            } catch(const std::exception& error) {
                renderer.clear();
                app.fail(i18n::tr("error.explorer_image", {{"detail", error.what()}}));
            }
        }
        app.update_previews();
        try {
            preview.update(app, now);
        } catch(const std::exception& error) {
            preview.clear();
            app.error = i18n::tr("error.preview_gpu", {{"detail", error.what()}});
            app.changed = true;
            std::cerr << app.error << '\n';
        }
        scene.update(app.scene_snapshot(now), hands, head, now);
        const auto models = controller_models.poll();
        for(unsigned hand = 0; hand < 2; ++hand) {
            const auto& model = models.hands[hand];
            const auto diagnostic =
                    std::to_string(static_cast<int>(model.status)) + ": " + model.detail + (model.model ? "; model=" + model.model->name + "; parts=" + std::to_string(model.model->parts.size()) : "");
            if(model_diagnostics[hand] != diagnostic) {
                model_diagnostics[hand] = diagnostic;
                std::cerr << "Controller model " << (hand ? "right" : "left") << ": " << diagnostic << '\n';
            }
        }
        try {
            renderer.update_controller_models(models);
        } catch(const std::exception& error) {
            std::cerr << "Controller model GPU: " << error.what() << '\n';
        }
        synchronize_passthrough(app, camera, true, session.passthrough_fault());
        renderer.passthrough(camera.enabled());
        const auto image = frame->acquire();
        renderer.draw(image, frame->views(), scene, app.presentation, 0);
        if(video_snapshot && renderer.ready()) {
            renderer.snapshot(*video_snapshot, frame->views()[0], scene, app.presentation, 0);
            video_snapshot.reset();
        }
        auto layers = panels.update(app, scene, now, hud_snapshot);
        const auto pictures = preview.layers(scene, app);
        layers.insert(layers.end(), pictures.begin(), pictures.end());
        const auto controller_layer = controllers.draw(frame->views(), scene);
        const auto headers = controller_headers(layers, controller_layer);
        frame->finish(headers, true, camera.blend_mode(), camera.layer());
        ++frames;
    }
    // The runtime asked us to exit, or a signal cut the loop short. Touching the XR
    // passthrough layer or destroying XR objects after that is what used to turn a clean
    // quit into a segmentation fault, so the runtime side is left alone from here on.
    // Quit from the viewer counts too. Conditioning this only on the runtime's exit
    // request left the button path destroying XR objects on a live runtime, which is the
    // crash it kept producing.
    // The decision, and why destroying the session is not on the table here, lives with the
    // session itself. application_run.cpp only reports what it can still touch.
    const bool instance_lost = session.instance_lost();
    leave_session(session);
    if(!instance_lost)
        synchronize_passthrough(app, camera, false, session.passthrough_fault());
    renderer.clear();
    preview.clear();
    // Stop the controller-model worker before the OpenXR session and the OpenVR
    // session are torn down. The worker can be inside a blocking OpenVR call, so
    // it must be joined while those sessions are still alive.
    controller_models.shutdown();
    // Close the OpenVR connection now, on this thread, rather than during static
    // destruction after main returns, which segfaults once SteamVR has gone.
    model_detail::shutdown_sessions();
    alarm(0);
    if(app.exit_requested())
        std::cerr << "Exit requested from the viewer; shutting down\n";
    if(options.stats)
        std::cerr << "XR: " << frames << " submitted frames in " << elapsed() << "s\n";
    if(hud_snapshot || video_snapshot)
        throw std::runtime_error("Requested snapshot was not rendered before shutdown");
    return 0;
}
}  // namespace lumaframevr::application
