#include "controller_layer.hpp"
#include "openxr_swapchain.hpp"
#include "vulkan_submission.hpp"
#include "video_surface.hpp"
#include <algorithm>
namespace lumaframevr::application {
ControllerLayer::ControllerLayer(OpenXrSession& session, const std::filesystem::path& shaders) : session_(session) {
    try {
        extent_ = {128, std::min(session.extent().height, 2048U)};
        const auto extent = extent_;
        XrSwapchainCreateInfo create{};
        create.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        create.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        create.format = session.format();
        create.sampleCount = 1;
        create.width = extent.width;
        create.height = extent.height;
        create.faceCount = 1;
        create.arraySize = 4;
        create.mipCount = 1;
        xr_check(xrCreateSwapchain(session.handle(), &create, &chain_), "create transparent controller swapchain");
        const auto images = swapchain_images(chain_, "enumerate controller images");
        const auto count = images.size();
        targets_.resize(count);
        for(unsigned index = 0; index < count; ++index)
            for(unsigned surface = 0; surface < 4; ++surface)
                targets_[index][surface] = std::make_unique<VulkanRenderTarget>(session.graphics(), images[index].image, extent, session.format(), surface);
        pass_ = std::make_unique<ControllerPass>(session.graphics(), targets_.front()[0]->render_pass(), shaders);
    } catch(...) {
        reset();
        throw;
    }
}
void ControllerLayer::reset() noexcept {
    pass_.reset();
    targets_.clear();
    if(chain_) {
        xrDestroySwapchain(chain_);
        chain_ = XR_NULL_HANDLE;
    }
}
ControllerLayer::~ControllerLayer() {
    reset();
}
std::vector<XrCompositionLayerQuad> ControllerLayer::draw(const std::array<XrView, 2>& views, const Scene& scene) {
    const auto controls = scene.overlay_payloads({}, {-1, 1, -1, 1});
    Pose viewer = pose_of(views[0].pose);
    viewer.position = mix(viewer.position, pose_of(views[1].pose).position, .5F);
    viewer.rotation = interpolate(viewer.rotation, pose_of(views[1].pose).rotation, .5F);
    std::vector<BeamSurface> surfaces;
    for(const auto& beam : controls.beams)
        if(auto surface = beam_surface(beam, viewer)) {
            surfaces.push_back(*surface);
            if(auto marker = beam_reticle(beam, viewer))
                surfaces.push_back(*marker);
        }
    if(surfaces.empty())
        return {};
    AcquiredSwapchainImage image(chain_, "acquire controller image");
    image.wait("wait controller image");
    {
        VulkanSubmission submission(session_.graphics());
        for(unsigned beam = 0; beam < surfaces.size(); ++beam) pass_->draw(submission.command(), *targets_.at(image.index())[beam], surfaces[beam]);
        submission.submit();
        submission.wait();
    }
    image.release("release controller image");
    return beam_quads(session_.local_space(), chain_, extent_, surfaces);
}
}  // namespace lumaframevr::application
