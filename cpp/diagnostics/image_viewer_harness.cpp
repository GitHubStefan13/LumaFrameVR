#include "application_scene.hpp"
#include "application_assets.hpp"
#include "render_probe.hpp"
#include "vulkan_device.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
namespace {
using namespace lumaframevr;
using namespace lumaframevr::application;
PanelKind kind(const std::string& name) {
    if(name == "browser")
        return PanelKind::browser;
    if(name == "transport")
        return PanelKind::transport;
    if(name == "settings")
        return PanelKind::settings;
    if(name == "rotation")
        return PanelKind::rotation;
    if(name == "information")
        return PanelKind::information;
    throw std::invalid_argument("unknown panel");
}
class Harness {
   public:
    explicit Harness(const ApplicationOptions& options) : app_(options), renderer_(VulkanDevice::create(), shader_directory().string()) { pump(); }
    void pump(const ControllerState& input = {}) {
        app_.update(input, {}, now());
        app_.synchronize([&] {
            renderer_.clear_frame();
            image_.reset();
        });
        if(auto image = app_.take_image()) {
            image_ = std::move(*image);
            renderer_.draw(*image_, app_.presentation);
            app_.uploaded();
        }
        app_.frame();
        app_.update_previews();
        scene_.update(app_.scene_snapshot(now()), input, Pose{}, now());
    }
    void wait(unsigned milliseconds) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        do {
            pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while(std::chrono::steady_clock::now() < until);
    }
    void state() const {
        const auto size = app_.image_dimensions();
        std::cout << "STATE path=" << std::quoted(app_.path.string()) << " image=" << app_.viewing_image() << " loading=" << app_.image_loading() << " dimensions=" << size[0] << 'x' << size[1]
                  << " playing=" << app_.playing() << " error=" << std::quoted(app_.error) << std::endl;
    }
    void panel(const std::string& name, const std::filesystem::path& path) {
        const auto& surface = find(kind(name));
        draw_panel(app_.panel_snapshot(), surface, now()).save_ppm(path);
    }
    void image(const std::filesystem::path& path) {
        if(!image_ || !app_.viewing_image())
            throw std::runtime_error("no active decoded photo");
        renderer_.draw(*image_, app_.presentation);
        renderer_.save(path);
    }
    void click(const std::string& name, int x, int y) {
        const auto& panel = find(kind(name));
        const Hit hit{.panel = panel.kind, .width = panel.pixels[0], .height = panel.pixels[1], .x = x, .y = y, .slot = panel.slot};
        app_.pointer(0, hit, true, now());
        app_.pointer(0, hit, false, now());
        pump();
    }
    void button(unsigned index) {
        ControllerState input;
        input.pressed.at(index) = true;
        pump(input);
        pump();
    }

   private:
    double now() const { return 5 + std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count(); }
    const ScenePanel& find(PanelKind desired) const {
        for(const auto& panel : scene_.panels())
            if(panel.kind == desired)
                return panel;
        throw std::runtime_error("panel is not currently visible");
    }
    std::chrono::steady_clock::time_point started_{std::chrono::steady_clock::now()};
    Controller app_;
    RenderProbe renderer_;
    Scene scene_;
    std::optional<ImagePixels> image_;
};
}  // namespace
int main(int argc, char** argv) {
    try {
        if(argc < 2)
            throw std::invalid_argument("image_viewer_harness FILE|DIRECTORY [--smoke OUTPUT.ppm]");
        const std::array<std::string_view, 1> arguments{argv[1]};
        Harness harness(parse_application_options(arguments));
        if(argc == 4 && std::string_view(argv[2]) == "--smoke") {
            harness.wait(100);
            harness.image(argv[3]);
            harness.state();
            return 0;
        }
        std::cout << "READY: wait MS; state; panel KIND PATH; image PATH; click KIND X Y; button INDEX; quit" << std::endl;
        std::string line;
        while(std::getline(std::cin, line)) {
            std::istringstream input(line);
            std::string command;
            input >> command;
            if(command == "quit")
                break;
            if(command == "wait") {
                unsigned duration;
                if(!(input >> duration) || duration > 10000)
                    throw std::invalid_argument("wait 0..10000 ms");
                harness.wait(duration);
            } else if(command == "state")
                harness.state();
            else if(command == "button") {
                unsigned index;
                if(!(input >> index))
                    throw std::invalid_argument("button index");
                harness.button(index);
            } else if(command == "click") {
                std::string panel;
                int x, y;
                if(!(input >> panel >> x >> y))
                    throw std::invalid_argument("click panel x y");
                harness.click(panel, x, y);
            } else if(command == "panel" || command == "image") {
                std::string panel, path;
                if(command == "panel")
                    input >> panel;
                if(!(input >> std::quoted(path)))
                    throw std::invalid_argument("output path");
                if(command == "panel")
                    harness.panel(panel, path);
                else
                    harness.image(path);
            } else
                throw std::invalid_argument("unknown command");
            std::cout << "OK " << command << std::endl;
        }
        return 0;
    } catch(const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
