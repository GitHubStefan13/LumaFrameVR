#include "openxr_composition.hpp"
namespace lumaframevr {
std::vector<const XrCompositionLayerBaseHeader*> frame_layers(XrCompositionLayerProjection& projection, bool project, std::span<const XrCompositionLayerBaseHeader* const> overlays,
                                                              const XrCompositionLayerBaseHeader* underlay, bool transparent) {
    projection.layerFlags = transparent ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
    std::vector<const XrCompositionLayerBaseHeader*> layers;
    if(underlay)
        layers.push_back(underlay);
    if(project)
        layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection));
    layers.insert(layers.end(), overlays.begin(), overlays.end());
    return layers;
}
}  // namespace lumaframevr
