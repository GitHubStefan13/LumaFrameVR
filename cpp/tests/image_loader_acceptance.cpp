#include "test_registry.hpp"
#include "image_loader.hpp"
#include "image_viewing.hpp"
#include "application_controller.hpp"
#include "application_scene.hpp"
#include "canvas.hpp"
#include "catalogue.hpp"
#include "media_decoder_startup.hpp"
#include "test_assertions.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <thread>
int lumaframevr::tests::image_loader(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        using namespace std::chrono_literals;
        std::atomic<bool> entered{}, superseded{};
        std::atomic<int> worker_priority{-99}, worker_threads{-1};
        ImageLoader loader([&](const std::filesystem::path& path, const std::function<bool()>& cancelled) {
            worker_priority = getpriority(PRIO_PROCESS, 0);
            worker_threads = media::software_decoder_thread_count();
            if(path == "old.png") {
                entered = true;
                while(!cancelled()) std::this_thread::sleep_for(1ms);
                superseded = true;
                throw std::runtime_error("cancelled old request");
            }
            if(path == "bad.png")
                throw std::runtime_error("bad image");
            return ImagePixels{1, 1, {255, 0, 0, 255}};
        });
        const auto wait = [](auto condition) {
            const auto until = std::chrono::steady_clock::now() + 3s;
            while(!condition()) {
                require(std::chrono::steady_clock::now() < until, "image loader must finish promptly");
                std::this_thread::sleep_for(1ms);
            }
        };
        loader.request("old.png");
        wait([&] { return entered.load(); });
        loader.request("new.png");
        std::optional<ImageResult> result;
        wait([&] {
            result = loader.take();
            return result.has_value();
        });
        require(superseded && result->path == "new.png" && result->pixels && result->error.empty(), "superseded image is cancelled and never published");
        require(!loader.take(), "completed image is consumed once");
        require(media::software_decoder_thread_count() == 0, "work outside a preview worker keeps FFmpeg's automatic decoder thread count");
        require(worker_priority.load() > 0, "a preview worker runs at a lowered scheduling priority");
        require(worker_threads.load() == media::preview_decoder_threads, "a preview decode bounds its decoder threads instead of taking every core");
        loader.request("bad.png");
        wait([&] {
            result = loader.take();
            return result.has_value();
        });
        require(!result->pixels && result->error == "bad image", "image failure retains actionable error");
        loader.request("new.png");
        loader.clear();
        std::this_thread::sleep_for(25ms);
        require(!loader.take(), "cleared loader never retains late image results");

        // Two paths whose reader failures are distinguishable: a file that is not an image, and
        // a directory, which the reader rejects before it ever reaches the decoder.
        const auto root = std::filesystem::temp_directory_path() / "lumaframevr-image-open";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "not-an-image.png");
        const auto broken = root / "broken.png";
        const auto rejected = root / "not-an-image.png";
        std::ofstream(broken, std::ios::trunc) << "this is not a png";
        const auto pump = [](application::ImageViewing& images) {
            const auto until = std::chrono::steady_clock::now() + 3s;
            while(images.loading()) {
                require(std::chrono::steady_clock::now() < until, "an image open must settle promptly");
                images.update();
                std::this_thread::sleep_for(1ms);
            }
        };
        {
            application::ImageViewing images;
            images.open(broken);
            pump(images);
            require(images.active() && !images.error().empty(), "a failed open keeps the active path and surfaces its failure");
            require(!images.update(), "a consumed result is never published twice");
            images.open(rejected);
            require(images.error().empty(), "a new open clears the previous failure before its own settles");
            pump(images);
            require(images.error().find("not a regular file") != std::string::npos, "a superseded request cannot publish over the current one");
            images.stop();
            require(!images.update() && images.error().empty(), "a stopped view publishes nothing and keeps no failure");
        }
        {
            ApplicationOptions options;
            options.file = broken;
            application::Controller controller(options);
            require(controller.viewing_image() && controller.image_loading(), "a directly opened image starts loading");
            require(controller.panels()[2] == application::PanelKind::information, "an opening still image is visible even without the playback HUD");
            for(int frame = 0; controller.error.empty(); ++frame) {
                require(static_cast<double>(frame) * .01 < 5, "a failing image open must surface promptly");
                controller.update({}, {}, frame * .01);
                std::this_thread::sleep_for(1ms);
            }
            const auto prefix = i18n::tr("error.explorer_image", {{"detail", ""}});
            require(controller.error.starts_with(prefix) && controller.error.size() > prefix.size(), "a failed image open surfaces the shared image error with its detail");
            require(!controller.viewing_image() && controller.panels()[2] == application::PanelKind::browser, "a failed image open returns to the browser that carries the message");
            application::ScenePanel browser_panel{application::PanelKind::browser, {{{0, 0, -2}, {0, 0, 0, 1}}, {3.84F, 2.02F}}, {1520, 800}, 2, {}};
            const auto with_error = application::draw_panel(controller.panel_snapshot(), browser_panel, 1.0);
            const auto message = controller.error;
            controller.error.clear();
            const auto without_error = application::draw_panel(controller.panel_snapshot(), browser_panel, 1.0);
            require(!std::equal(with_error.pixels().begin(), with_error.pixels().end(), without_error.pixels().begin()), "the browser surface actually draws the failure message");
            controller.error = message;
        }
        std::filesystem::remove_all(root);
        std::cout << "Latest image wins; supersession, cancellation, error propagation, clear, preview worker decode budget and the surfaced image-open failure verified\n";
    });
}
