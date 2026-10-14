#pragma once
#include <openxr/openxr.h>
#include <span>
#include <vector>
namespace lumaframevr {
std::vector<const XrCompositionLayerBaseHeader*> frame_layers(XrCompositionLayerProjection& projection, bool project, std::span<const XrCompositionLayerBaseHeader* const> overlays,
                                                              const XrCompositionLayerBaseHeader* underlay, bool transparent);
}
