#version 450
layout(binding = 0) uniform sampler2D model_texture;
layout(location = 0) in vec2 texture_uv;
layout(location = 1) in vec3 eye_normal;
layout(location = 0) out vec4 colour;
void main() {
    vec4 material = texture(model_texture, texture_uv);
    if (material.a < 0.01) discard;
    float light = 0.65 + 0.35 * max(dot(normalize(eye_normal), normalize(vec3(0.2, 0.4, 1.0))), 0.0);
    colour = vec4(material.rgb * light, material.a);
}
