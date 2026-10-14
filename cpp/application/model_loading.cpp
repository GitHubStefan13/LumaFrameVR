#include "model_openvr.hpp"
#include <algorithm>
#include <stdexcept>
namespace lumaframevr::application::model_detail {
namespace {
void check_error(EVRRenderModelError error, const std::string& context) {
    if(error != EVRRenderModelError_VRRenderModelError_None)
        throw std::runtime_error("OpenVR " + context + " failed: " + std::to_string(error));
}
ControllerModel copy_model(const RenderModel_t& mesh, const RenderModel_TextureMap_t& texture) {
    if(!mesh.rVertexData || !mesh.rIndexData || !mesh.unVertexCount || mesh.unVertexCount > 65536 || !mesh.unTriangleCount || mesh.unTriangleCount > 1000000)
        throw std::runtime_error("invalid OpenVR mesh data");
    if(texture.format != EVRRenderModelTextureFormat_VRRenderModelTextureFormat_RGBA8_SRGB)
        throw std::runtime_error("unsupported OpenVR texture format: " + std::to_string(texture.format));
    if(!texture.rubTextureMapData || !texture.unWidth || !texture.unHeight || texture.unWidth > 16384 || texture.unHeight > 16384)
        throw std::runtime_error("invalid OpenVR RGBA texture");
    ControllerModel result;
    result.vertices.reserve(mesh.unVertexCount);
    for(uint32_t i = 0; i < mesh.unVertexCount; ++i) {
        const auto& v = mesh.rVertexData[i];
        result.vertices.push_back({{v.vPosition.v[0], v.vPosition.v[1], v.vPosition.v[2]}, {v.vNormal.v[0], v.vNormal.v[1], v.vNormal.v[2]}, {v.rfTextureCoord[0], v.rfTextureCoord[1]}});
    }
    result.indices.assign(mesh.rIndexData, mesh.rIndexData + std::size_t(mesh.unTriangleCount) * 3);
    result.width = texture.unWidth;
    result.height = texture.unHeight;
    result.rgba.assign(texture.rubTextureMapData, texture.rubTextureMapData + std::size_t(result.width) * result.height * 4);
    return result;
}
}  // namespace
ModelLoadStatus load_part(const std::shared_ptr<Session>& session, HandWork& hand, PartWork& part) {
    if(part.result)
        return ModelLoadStatus::Ready;
    auto* api = session->models;
    if(!part.mesh) {
        RenderModel_t* raw{};
        auto error = api->LoadRenderModel_Async(part.name.data(), &raw);
        if(raw)
            part.mesh = {raw, [session](RenderModel_t* value) { session->models->FreeRenderModel(value); }};
        if(error == EVRRenderModelError_VRRenderModelError_Loading)
            return ModelLoadStatus::LoadingModel;
        check_error(error, "mesh " + part.name);
        if(!part.mesh)
            throw std::runtime_error("OpenVR returned null mesh: " + part.name);
    }
    RenderModel_TextureMap_t* raw{};
    auto error = api->LoadTexture_Async(part.mesh->diffuseTextureId, &raw);
    std::shared_ptr<RenderModel_TextureMap_t> texture;
    if(raw)
        texture = {raw, [session](RenderModel_TextureMap_t* value) { session->models->FreeTexture(value); }};
    if(error == EVRRenderModelError_VRRenderModelError_Loading)
        return ModelLoadStatus::LoadingTexture;
    check_error(error, "texture " + part.name);
    if(!texture)
        throw std::runtime_error("OpenVR returned null texture: " + part.name);
    auto result = copy_model(*part.mesh, *texture);
    result.name = part.name;
    result.provenance = "SteamVR IVRRenderModels_006 runtime device=" + std::to_string(hand.device) + " root=" + hand.name + " component=" + part.component;
    result.visible = part.visible;
    auto transform = part.model_from_part;
    if(hand.grip_from_model) {
        transform = multiply(*hand.grip_from_model, transform);
        result.grip_from_model = identity();
    }
    transform_vertices(result, transform);
    if(auto issue = validate_controller_model(result); !issue.empty())
        throw std::runtime_error("invalid OpenVR model: " + issue);
    part.result = std::move(result);
    part.mesh.reset();
    return ModelLoadStatus::Ready;
}
}  // namespace lumaframevr::application::model_detail
