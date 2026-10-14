#include "beam_surface.hpp"
#include "video_surface.hpp"
#include <algorithm>
namespace lumaframevr::application {
namespace {
Vector3 cross(Vector3 a, Vector3 b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vector3 unit(Vector3 v) {
    const auto magnitude = length(v);
    for(auto& axis : v) axis /= magnitude;
    return v;
}
Quaternion basis(Vector3 x, Vector3 y, Vector3 z) {
    const std::array<std::array<float, 3>, 3> m{{{x[0], y[0], z[0]}, {x[1], y[1], z[1]}, {x[2], y[2], z[2]}}};
    Quaternion q;
    const auto trace = m[0][0] + m[1][1] + m[2][2];
    if(trace > 0) {
        const float scale = 2 * std::sqrt(trace + 1);
        q = {(m[2][1] - m[1][2]) / scale, (m[0][2] - m[2][0]) / scale, (m[1][0] - m[0][1]) / scale, scale / 4};
    } else {
        unsigned i = m[1][1] > m[0][0] ? 1 : 0;
        if(m[2][2] > m[i][i])
            i = 2;
        const unsigned j = (i + 1) % 3, k = (i + 2) % 3;
        const float scale = 2 * std::sqrt(1 + m[i][i] - m[j][j] - m[k][k]);
        q[i] = scale / 4;
        q[j] = (m[j][i] + m[i][j]) / scale;
        q[k] = (m[k][i] + m[i][k]) / scale;
        q[3] = (m[k][j] - m[j][k]) / scale;
    }
    return normalize(q);
}
}  // namespace
std::optional<BeamSurface> beam_surface(const OverlayPayload& beam, Pose viewer) {
    if(!(beam.values[5][1] > 0))
        return {};
    Vector3 start, end, direction;
    for(unsigned axis = 0; axis < 3; ++axis) {
        start[axis] = beam.values[0][axis];
        end[axis] = beam.values[1][axis];
        direction[axis] = end[axis] - start[axis];
        if(!std::isfinite(start[axis]) || !std::isfinite(end[axis]))
            return {};
    }
    const auto distance = length(direction);
    if(distance < 1e-4F)
        return {};
    const auto y = unit(direction);
    BeamSurface surface;
    surface.pose.position = mix(start, end, .5F);
    Vector3 facing;
    for(unsigned axis = 0; axis < 3; ++axis) facing[axis] = viewer.position[axis] - surface.pose.position[axis];
    auto remove_along = [&](Vector3 v) {
        const auto along = dot(v, y);
        for(unsigned axis = 0; axis < 3; ++axis) v[axis] -= along * y[axis];
        return v;
    };
    auto normal = remove_along(facing);
    if(length(normal) < 1e-4F)
        normal = remove_along(rotate(viewer.rotation, {0, 1, 0}));
    if(length(normal) < 1e-4F)
        normal = remove_along(rotate(viewer.rotation, {1, 0, 0}));
    const auto z = unit(normal), x = unit(cross(y, z));
    surface.pose.rotation = basis(x, y, z);
    const float margin = std::max({beam.values[5][0], beam.values[5][2] * 1.1F, .0001F}) * 1.25F;
    surface.size = {margin * 2, distance + margin * 2};
    surface.raster = beam;
    surface.raster.values[0] = {distance, 0, 0, 0};
    surface.raster.values[4] = {surface.size[0], surface.size[1], 0, 0};
    return surface;
}
std::optional<BeamSurface> beam_reticle(const OverlayPayload& beam, Pose viewer) {
    if(!(beam.values[5][1] > 0 && beam.values[5][2] > 0))
        return {};
    BeamSurface marker;
    Vector3 facing;
    for(unsigned axis = 0; axis < 3; ++axis) {
        marker.pose.position[axis] = beam.values[1][axis];
        if(!std::isfinite(marker.pose.position[axis]))
            return {};
        facing[axis] = viewer.position[axis] - marker.pose.position[axis];
    }
    const auto z = length(facing) > 1e-4F ? unit(facing) : rotate(viewer.rotation, {0, 0, 1});
    auto up = rotate(viewer.rotation, {0, 1, 0});
    const auto along = dot(up, z);
    for(unsigned axis = 0; axis < 3; ++axis) up[axis] -= along * z[axis];
    if(length(up) < 1e-4F)
        up = cross(z, rotate(viewer.rotation, {1, 0, 0}));
    const auto y = unit(up), x = unit(cross(y, z));
    marker.pose.rotation = basis(x, y, z);
    const float width = beam.values[5][2] * 1.1F * 2 * 1.25F;
    marker.size = {width, width};
    marker.raster = beam;
    marker.raster.values[0] = {0, 1, 0, 0};
    marker.raster.values[4] = {width, width, 0, 0};
    return marker;
}
std::vector<XrCompositionLayerQuad> beam_quads(XrSpace space, XrSwapchain chain, VkExtent2D extent, std::span<const BeamSurface> surfaces) {
    std::vector<XrCompositionLayerQuad> layers;
    for(unsigned index = 0; index < surfaces.size(); ++index) {
        const auto& surface = surfaces[index];
        XrCompositionLayerQuad layer{};
        layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layer.space = space;
        layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        layer.pose = xr_pose(surface.pose);
        layer.size = {surface.size[0], surface.size[1]};
        layer.subImage.swapchain = chain;
        layer.subImage.imageArrayIndex = index;
        layer.subImage.imageRect.extent = {static_cast<int>(extent.width), static_cast<int>(extent.height)};
        layers.push_back(layer);
    }
    return layers;
}
}  // namespace lumaframevr::application
