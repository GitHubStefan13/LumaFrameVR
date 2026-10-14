#version 450
layout(location = 0) in vec2 screen;
layout(location = 0) out vec4 out_colour;
layout(push_constant) uniform Pointer {
    vec4 geometry;
    vec4 unused_target;
    vec4 colour;
    vec4 unused_view;
    vec4 size;
    vec4 params;
} pointer;
void main() {
    vec2 point = screen * pointer.size.xy * 0.5;
    float segment = max(pointer.geometry.x, 1e-4);
    float t = clamp(point.y / segment + 0.5, 0.0, 1.0);
    float distance = length(point - vec2(0.0, (t - 0.5) * segment));
    float radius = max(pointer.params.x, 1e-4);
    float glow = 1.0 - smoothstep(radius * 0.3, radius, distance);
    float along = smoothstep(clamp(pointer.params.w, 0.0, 0.99), 1.0, t);
    float beam_alpha = pointer.colour.a * pointer.params.y;
    float reticle = pointer.params.z;
    if (reticle <= 0.0) {
        beam_alpha *= 1.0 - along;
        glow *= 1.0 - 0.6 * along;
    }
    float alpha = glow * beam_alpha;
    if (pointer.geometry.y == 1.0) {
        alpha = 0.0;
        float end_distance = length(point);
        float dot_glow = 1.0 - smoothstep(reticle * 0.45, reticle, end_distance);
        float ring = smoothstep(reticle * 0.55, reticle * 0.75, end_distance)
            * (1.0 - smoothstep(reticle * 0.9, reticle * 1.1, end_distance));
        alpha = max(alpha, (dot_glow + ring * 0.85) * pointer.params.y);
    }
    if (alpha <= 0.004) discard;
    out_colour = vec4(pointer.colour.rgb, alpha);
}
