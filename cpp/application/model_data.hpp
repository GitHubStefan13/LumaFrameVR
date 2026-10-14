#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace lumaframevr::application {
struct ModelVertex {
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 2> uv;
};
struct ControllerModel {
    std::vector<ModelVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba;
    std::string provenance, name;
    bool visible{true};
    std::optional<std::array<float, 16>> grip_from_model;
};
std::string validate_controller_model(const ControllerModel& model);
ControllerModel load_controller_fixture(const std::filesystem::path& path);
}  // namespace lumaframevr::application
