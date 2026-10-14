#include "application_run.hpp"
#include "media_file_type.hpp"
#include "application_assets.hpp"
#include "decoder_session.hpp"
#include "render_probe.hpp"
#include "remembered_video_settings.hpp"
#include "catalogue.hpp"
#include "vulkan_device.hpp"
#include <chrono>
#include <iostream>
namespace lumaframevr::application {
namespace {
void image_probe(const ApplicationOptions& options) {
    auto image = read_image(options.file);
    const auto width = image.width, height = image.height;
    RememberedVideoSettings settings(options.overrides);
    auto presentation = settings.open(options.file);
    settings.infer(options.file, {}, presentation);
    if(options.render_probe) {
        RenderProbe renderer(VulkanDevice::create(), shader_directory().string());
        renderer.draw(std::move(image), presentation);
        if(options.snapshot)
            renderer.save(*options.snapshot);
    }
    std::cerr << "Image probe: " << width << " x " << height << ", RGBA, render=" << std::boolalpha << options.render_probe << '\n';
}
}  // namespace
void probe(const ApplicationOptions& options) {
    if(!(options.source && options.source->remote()) && !supported_media_file(options.file))
        throw std::invalid_argument(i18n::tr("error.unsupported"));
    if(!(options.source && options.source->remote()) && is_image_path(options.file)) {
        image_probe(options);
        return;
    }
    DecoderSession decoder(options.input());
    std::unique_ptr<RenderProbe> renderer;
    if(options.render_probe)
        renderer = std::make_unique<RenderProbe>(shader_directory().string());
    const auto started = std::chrono::steady_clock::now();
    std::size_t frames{};
    for(;;) {
        if(decoder.reconfigure(false)) {
            if(renderer)
                renderer->clear_frame();
            decoder.reconfigure(true);
        }
        auto result = decoder.advance();
        if(result.status == DecodeStatus::end)
            break;
        if(result.status != DecodeStatus::picture)
            continue;
        if(renderer)
            renderer->draw(std::move(result.frame), options.presentation);
        ++frames;
        if(options.seconds && std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >= *options.seconds)
            break;
    }
    if(!frames)
        throw std::runtime_error("No frames decoded");
    if(renderer && options.snapshot)
        renderer->save(*options.snapshot);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cerr << "Probe: " << frames << " frames in " << elapsed << "s, render=" << std::boolalpha << options.render_probe << '\n';
}
}  // namespace lumaframevr::application
