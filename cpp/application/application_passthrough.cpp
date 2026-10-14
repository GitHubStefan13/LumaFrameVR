#include "application_passthrough.hpp"
#include <iostream>
namespace lumaframevr::application {
void synchronize_passthrough(Controller& app, OpenXrPassthrough& camera, bool renderable, bool fault) {
    const bool supported = camera.available() && !fault;
    const auto reason = fault ? std::string("XR runtime reported a camera passthrough fault") : camera.detail();
    if(app.passthrough_supported != supported || app.passthrough_reason != reason) {
        app.passthrough_supported = supported;
        app.passthrough_reason = reason;
        app.changed = true;
        ++app.revision;
    }
    if(app.passthrough_enabled && !supported)
        app.toggle_passthrough();
    try {
        camera.set_enabled(app.passthrough_enabled && supported && renderable);
    } catch(const std::exception& error) {
        app.passthrough_supported = false;
        app.passthrough_reason = error.what();
        app.toggle_passthrough();
        try {
            camera.set_enabled(false);
        } catch(const std::exception& pause) {
            std::cerr << pause.what() << '\n';
        }
        std::cerr << app.error << '\n';
    }
}
}  // namespace lumaframevr::application
