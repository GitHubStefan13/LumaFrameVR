#include "model_openvr.hpp"
#include <cmath>
#include <stdexcept>
namespace lumaframevr::application::model_detail {
Matrix identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
Matrix matrix_from(const HmdMatrix34_t& source) {
    Matrix result = identity();
    for(std::size_t r = 0; r < 3; ++r)
        for(std::size_t c = 0; c < 4; ++c) result[c * 4 + r] = source.m[r][c];
    for(auto value : result)
        if(!std::isfinite(value))
            throw std::runtime_error("nonfinite OpenVR component transform");
    return result;
}
Matrix inverse_rigid(const Matrix& source) {
    for(std::size_t a = 0; a < 3; ++a)
        for(std::size_t b = 0; b < 3; ++b) {
            float dot{};
            for(std::size_t k = 0; k < 3; ++k) dot += source[a * 4 + k] * source[b * 4 + k];
            if(std::abs(dot - float(a == b)) > 0.01f)
                throw std::runtime_error("nonrigid OpenVR component transform");
        }
    Matrix result = identity();
    for(std::size_t r = 0; r < 3; ++r) {
        for(std::size_t c = 0; c < 3; ++c) result[c * 4 + r] = source[r * 4 + c];
        for(std::size_t c = 0; c < 3; ++c) result[12 + r] -= result[c * 4 + r] * source[12 + c];
    }
    return result;
}
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for(std::size_t r = 0; r < 4; ++r)
        for(std::size_t c = 0; c < 4; ++c)
            for(std::size_t k = 0; k < 4; ++k) result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    return result;
}
void transform_vertices(ControllerModel& model, const Matrix& transform) {
    inverse_rigid(transform);
    for(auto& vertex : model.vertices) {
        auto position = vertex.position, normal = vertex.normal;
        for(std::size_t r = 0; r < 3; ++r) {
            vertex.position[r] = transform[12 + r];
            vertex.normal[r] = 0;
            for(std::size_t c = 0; c < 3; ++c) {
                vertex.position[r] += transform[c * 4 + r] * position[c];
                vertex.normal[r] += transform[c * 4 + r] * normal[c];
            }
        }
    }
}
}  // namespace lumaframevr::application::model_detail
