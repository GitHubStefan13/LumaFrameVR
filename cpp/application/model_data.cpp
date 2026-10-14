#include "model_data.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace lumaframevr::application {
std::string validate_controller_model(const ControllerModel& model) {
    if(model.vertices.empty() || model.vertices.size() > 65536)
        return "invalid vertex count";
    if(model.indices.empty() || model.indices.size() % 3)
        return "invalid triangle indices";
    for(auto index : model.indices)
        if(index >= model.vertices.size())
            return "index exceeds vertex count";
    for(const auto& vertex : model.vertices) {
        for(auto value : vertex.position)
            if(!std::isfinite(value))
                return "nonfinite position";
        for(auto value : vertex.normal)
            if(!std::isfinite(value))
                return "nonfinite normal";
        for(auto value : vertex.uv)
            if(!std::isfinite(value))
                return "nonfinite UV";
    }
    if(!model.width || !model.height || model.width > 16384 || model.height > 16384)
        return "invalid texture dimensions";
    if(model.rgba.size() != std::uint64_t(model.width) * model.height * 4)
        return "invalid RGBA texture size";
    if(model.grip_from_model)
        for(auto value : *model.grip_from_model)
            if(!std::isfinite(value))
                return "nonfinite grip transform";
    return {};
}
namespace {
struct Reader {
    std::ifstream stream;
    explicit Reader(const std::filesystem::path& path) : stream(path, std::ios::binary) {
        if(!stream)
            throw std::runtime_error("cannot open controller fixture: " + path.string());
    }
    void bytes(char* output, std::size_t count) {
        if(!stream.read(output, static_cast<std::streamsize>(count)))
            throw std::runtime_error("truncated controller fixture");
    }
    std::uint32_t integer(unsigned count = 4) {
        std::uint32_t result{};
        for(unsigned i = 0; i < count; ++i) {
            char value{};
            bytes(&value, 1);
            result |= std::uint32_t(static_cast<unsigned char>(value)) << (i * 8);
        }
        return result;
    }
    float scalar() { return std::bit_cast<float>(integer()); }
    std::string text() {
        auto size = integer();
        if(size > 4096)
            throw std::runtime_error("oversized fixture metadata");
        std::string result(size, '\0');
        bytes(result.data(), size);
        return result;
    }
};
}  // namespace
ControllerModel load_controller_fixture(const std::filesystem::path& path) {
    Reader input(path);
    std::array<char, 8> magic{};
    input.bytes(magic.data(), magic.size());
    if(std::string(magic.data(), magic.size()) != "LFCMODEL")
        throw std::runtime_error("invalid controller fixture magic");
    auto vertices = input.integer(), indices = input.integer();
    ControllerModel result;
    result.width = input.integer();
    result.height = input.integer();
    if(!vertices || vertices > 65536 || !indices || indices > 3000000 || !result.width || result.width > 4096 || !result.height || result.height > 4096)
        throw std::runtime_error("invalid controller fixture dimensions");
    result.provenance = input.text();
    result.name = input.text();
    auto matrix = input.integer();
    if(matrix > 1)
        throw std::runtime_error("invalid fixture matrix flag");
    if(matrix) {
        result.grip_from_model.emplace();
        for(auto& value : *result.grip_from_model) value = input.scalar();
    }
    result.vertices.resize(vertices);
    for(auto& vertex : result.vertices) {
        for(auto& value : vertex.position) value = input.scalar();
        for(auto& value : vertex.normal) value = input.scalar();
        for(auto& value : vertex.uv) value = input.scalar();
    }
    result.indices.resize(indices);
    for(auto& index : result.indices) index = static_cast<std::uint16_t>(input.integer(2));
    result.rgba.resize(std::size_t(result.width) * result.height * 4);
    input.bytes(reinterpret_cast<char*>(result.rgba.data()), result.rgba.size());
    if(input.stream.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("trailing fixture data");
    if(auto error = validate_controller_model(result); !error.empty())
        throw std::runtime_error("invalid controller fixture: " + error);
    return result;
}
}  // namespace lumaframevr::application
