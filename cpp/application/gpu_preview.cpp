#include "gpu_preview.hpp"
#include "openxr_swapchain.hpp"
#include "vulkan_submission.hpp"
#include <numbers>
#include "preview_layout.hpp"
namespace lumaframevr::application {
GpuPreview::GpuPreview(OpenXrSession& session, const std::filesystem::path& shaders) : session_(session) {
    try {
        XrSwapchainCreateInfo create{};
        create.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        create.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        create.format = session.format();
        create.sampleCount = 1;
        create.width = preview_extent[0];
        create.height = preview_extent[1];
        create.faceCount = 1;
        create.arraySize = 1;
        create.mipCount = 1;
        xr_check(xrCreateSwapchain(session.handle(), &create, &chain_), "create GPU preview swapchain");
        images_ = swapchain_images(chain_, "enumerate GPU preview images");
        for(const auto& image : images_)
            targets_.push_back(std::make_unique<VulkanRenderTarget>(session.graphics(), image.image, VkExtent2D{preview_extent[0], preview_extent[1]}, session.format(), 0));
        video_ = std::make_unique<VideoSurface>(session.graphics(), targets_.front()->render_pass(), shaders);
    } catch(...) {
        reset();
        throw;
    }
}
void GpuPreview::reset() noexcept {
    video_.reset();
    targets_.clear();
    if(chain_)
        xrDestroySwapchain(chain_);
}
GpuPreview::~GpuPreview() {
    reset();
}
void GpuPreview::clear() {
    video_->clear();
    shown_ = false;
    selection_.clear();
}
void GpuPreview::render(const VideoPresentation& presentation) {
    AcquiredSwapchainImage image(chain_, "acquire preview image");
    try {
        image.wait("wait preview image");
        XrView view{};
        view.type = XR_TYPE_VIEW;
        view.pose.orientation.w = 1;
        const float horizontal = std::numbers::pi_v<float> / 4, vertical = std::atan(static_cast<float>(preview_extent[1]) / preview_extent[0]);
        view.fov = {-horizontal, horizontal, vertical, -vertical};
        VulkanSubmission submission(session_.graphics());
        auto& target = *targets_.at(image.index());
        target.begin(submission.command());
        video_->draw(submission.command(), target.extent(), view, 0, presentation, 0);
        target.end(submission.command());
        submission.submit();
        submission.wait();
    } catch(...) {
        image.release_unchecked();
        video_->clear();
        throw;
    }
    image.release("release preview image");
    video_->clear();
    shown_ = true;
}
void GpuPreview::update(Controller& app, double now) {
    const bool explorer = !app.playing();
    const auto path = app.explorer.browser.selected_path();
    const auto layout = preview_layout({explorer, explorer_, selection_ != path, app.transport_visible(now), shown_, 0, {}});
    if(layout.clear)
        clear();
    explorer_ = explorer;
    selection_ = path;
    auto frame = app.take_preview();
    if(frame.valid() && layout.visible) {
        video_->upload(std::move(frame));
        render(app.presentation);
    }
    if(auto photo = app.explorer.take_image_preview(); photo && explorer && layout.visible) {
        video_->upload(std::move(*photo));
        render(VideoPresentation::flat());
    }
}
std::vector<XrCompositionLayerQuad> GpuPreview::layers(const Scene& scene, const Controller& app) const {
    std::vector<XrCompositionLayerQuad> output;
    if(!shown_)
        return output;
    for(const auto& panel : scene.panels()) {
        const auto transport = app.transport();
        const auto layout = preview_layout({explorer_, explorer_, false, true, shown_, transport.position, transport.duration}, PreviewPanel{panel.kind, panel.pixels, panel.surface.size});
        if(!layout.placement)
            continue;
        const auto& placement = *layout.placement;
        XrCompositionLayerQuad layer{};
        layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        layer.space = session_.local_space();
        layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        layer.pose = xr_pose(compose(panel.surface.pose, placement.offset));
        layer.size = {placement.width, placement.height};
        layer.subImage.swapchain = chain_;
        layer.subImage.imageRect.extent = {preview_extent[0], preview_extent[1]};
        output.push_back(layer);
    }
    return output;
}
}  // namespace lumaframevr::application
