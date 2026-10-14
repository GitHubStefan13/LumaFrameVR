#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec2 texture_uv;
layout(location = 1) out vec3 eye_normal;
layout(push_constant) uniform Controller {
    vec4 translation;
    vec4 rotation;
    vec4 unused_size;
    vec4 unused_colour;
    vec4 view;
    vec4 fov;
} controller;
vec3 rotate(vec3 v, vec4 q) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
void main() {
    vec3 point = rotate(rotate(position, controller.rotation) + controller.translation.xyz, controller.view);
    float depth = -point.z;
    float x = (2.0 * point.x - depth * (controller.fov.y + controller.fov.x)) / (controller.fov.y - controller.fov.x);
    float y = (2.0 * point.y - depth * (controller.fov.w + controller.fov.z)) / (controller.fov.w - controller.fov.z);
    float near_plane = 0.01, far_plane = 100.0;
    float z = far_plane / (far_plane - near_plane) * depth - far_plane * near_plane / (far_plane - near_plane);
    gl_Position = vec4(x, y, z, depth);
    texture_uv = uv;
    eye_normal = rotate(rotate(normal, controller.rotation), controller.view);
}
