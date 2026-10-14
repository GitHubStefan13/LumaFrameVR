#include "decode_probe.hpp"
#include "render_probe.hpp"
#include "decoder_session.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace lumaframevr {
ProbeResult run_decode_probe(const ApplicationOptions& options) {
    DecoderSession decoder(options.input());
    std::unique_ptr<RenderProbe> render;
    if(options.render_probe)
        render = std::make_unique<RenderProbe>(LUMAFRAMEVR_SHADER_DIRECTORY);
    const auto started = std::chrono::steady_clock::now();
    ProbeResult result;
    for(;;) {
        if(decoder.reconfigure(false)) {
            if(render)
                render->clear_frame();
            decoder.reconfigure(true);
        }
        auto decoded = decoder.advance();
        if(decoded.status == DecodeStatus::end)
            break;
        if(decoded.status != DecodeStatus::picture)
            continue;
        if(render)
            render->draw(std::move(decoded.frame), options.presentation);
        ++result.frames;
        if(options.seconds && std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >= *options.seconds)
            break;
    }
    if(render && result.frames && options.snapshot)
        render->save(*options.snapshot);
    result.elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cerr << "Probe: " << result.frames << " frames in " << std::fixed << std::setprecision(3) << result.elapsed << "s (" << std::setprecision(1)
              << static_cast<double>(result.frames) / result.elapsed << " fps), render=" << std::boolalpha << options.render_probe << '\n';
    if(!result.frames)
        throw std::runtime_error("No frames decoded");
    return result;
}
}  // namespace lumaframevr
