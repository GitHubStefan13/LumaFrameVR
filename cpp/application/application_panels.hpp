#pragma once
#include "application_scene.hpp"
#include "openxr_panel.hpp"
namespace lumaframevr::application {
class Panels {
   public:
    explicit Panels(OpenXrSession& session) : session_(session) {}
    std::vector<XrCompositionLayerQuad> update(Controller& app, const Scene& scene, double now, std::optional<std::filesystem::path>& snapshot);

   private:
    struct Slot {
        std::unique_ptr<OpenXrPanel> panel;
        std::array<int, 2> pixels{};
        std::optional<PanelKind> kind;
        std::uint64_t revision{};
        double painted{-1};
    };
    OpenXrSession& session_;
    std::array<Slot, 7> slots_;
};
}  // namespace lumaframevr::application
