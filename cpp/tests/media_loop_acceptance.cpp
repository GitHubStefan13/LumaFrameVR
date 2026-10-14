#include "test_registry.hpp"
#include "application_controller.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace lumaframevr;
using namespace lumaframevr::application;
static void check(bool value, const char* message) {
    if(!value)
        throw std::runtime_error(message);
}
template <class Predicate>
void wait(Predicate predicate) {
    for(int tries = 0; tries < 3000; ++tries) {
        if(predicate())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("real media deadline exceeded");
}
int lumaframevr::tests::media_loop(int argc, char** argv) {
    try {
        if(argc != 2 || !std::filesystem::is_regular_file(argv[1])) {
            std::cout << "SKIP: LUMAFRAMEVR_ASSEMBLY_VIDEO is not set\n";
            return 77;
        }
        const std::filesystem::path clip(argv[1]);
        ApplicationOptions options;
        options.file = clip;
        options.source = MediaSource::parse(clip.string());
        const auto begin = std::chrono::steady_clock::now();
        Controller app(options);
        check(std::chrono::steady_clock::now() - begin < std::chrono::milliseconds(100), "media opening off render thread");
        RetainedFrame selected;
        wait([&] {
            selected = app.frame();
            return selected.valid();
        });
        check(app.snapshot().queue_depth <= 2 && app.snapshot().decoder.name.size() > 0, "bounded real native decoder");
        app.transport_button(1, 1);
        const double paused = app.transport().position;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        check(!app.frame().valid() && app.transport().position == paused, "pause clock holds");
        selected = {};
        app.seek(2, 2);
        const auto generation = *app.seek_generation();
        bool released = false;
        wait([&] { return app.snapshot().release_required; });
        app.synchronize([&] { released = true; });
        check(released && app.snapshot().generation == generation, "production graphics release before seek acknowledgment");
        wait([&] {
            selected = app.frame();
            return selected.valid();
        });
        check(selected.pts() >= 1.9, "real paused seek target selected");
        selected = {};
        app.transport_button(1, 3);
        app.seek(1000, 4);
        wait([&] { return app.snapshot().release_required; });
        app.synchronize([] {});
        wait([&] {
            selected = app.frame();
            selected = {};
            return !app.playing();
        });
        app.synchronize([] {});
        wait([&] {
            app.update({}, {}, 5);
            return !app.retiring();
        });
        app.explorer.browser.go(clip.parent_path());
        wait([&] {
            app.update({}, {}, 6);
            return !app.explorer.browser.loading();
        });
        for(std::size_t index = 0; index < app.explorer.browser.entries().size(); ++index)
            if(app.explorer.browser.entries()[index].path == clip)
                app.explorer.browser.select(index);
        wait([&] {
            app.update_previews();
            selected = app.take_preview();
            return selected.valid();
        });
        check(selected.software(), "explorer preview is independently software decoded");
        check(app.explorer.tiles.sources() <= 256 && app.explorer.tiles.bytes() <= 48 * 1024 * 1024, "preview cache bounded in production routing");
        selected = {};
        app.select_entry(app.explorer.browser.selected());
        check(app.queued() == clip, "EOF restart queues exact selection");
        app.update({}, {}, 7);
        check(app.playing(), "EOF restart starts after retired decoder completes");
        wait([&] {
            selected = app.frame();
            return selected.valid();
        });
        ControllerState back;
        back.pressed[2] = true;
        app.update(back, {}, 8);
        check(!app.playing() && app.retiring(), "input stop retains decoder before graphics release");
        bool input_released = false;
        app.synchronize([&] {
            if(!input_released) {
                check(selected.valid(), "input retirement retains renderer frame until callback");
                selected = {};
                input_released = true;
            }
        });
        check(input_released, "input stop reaches lifetime coordination");
        std::cout << "PASS actual native lifecycle pause seek release EOS async retirement and explorer preview\n";
    } catch(const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
    return 0;
}
