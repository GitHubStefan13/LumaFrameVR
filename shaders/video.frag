#version 450
#extension GL_GOOGLE_include_directive : require
#include "video_color.glsl"
layout(location = 0) in vec2 position;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D video;
layout(push_constant) uniform Parameters {
    vec4 fov;
    vec4 rotation;
    vec4 framing;
    vec4 stereo_offset;
    float aspect;
    int projection;
    int stereo;
    int eye;
    vec4 lens;
    vec4 distortion;
    vec4 crop;
};
const float PI = 3.141592653589793;

void main() {
    const float background_alpha = (eye & 2) != 0 ? 0.0 : 1.0;
    const int video_eye = eye & 1;
    vec2 p = position * 0.5 + 0.5;
    vec3 ray = normalize(vec3(mix(fov.x, fov.y, p.x), mix(fov.z, fov.w, p.y), -1.0));
    ray += 2.0 * cross(rotation.xyz, cross(rotation.xyz, ray) + rotation.w * ray);
    if (framing.z != 1.0) ray = normalize(vec3(ray.xy * framing.z, ray.z));
    vec2 uv;
    if (projection == 0) {
        vec2 point = ray.xy / max(-ray.z, 0.00001) - framing.xy;
        uv = vec2(point.x, -point.y * aspect) * 0.5 + 0.5;
        if (ray.z >= 0.0) {
            color = vec4(0.0, 0.0, 0.0, background_alpha);
            return;
        }
    } else {
        if (any(notEqual(framing.xy, vec2(0.0)))) {
            vec3 center = vec3(framing.xy, 0.0);
            float along = dot(ray, center);
            ray = normalize(ray * (along + sqrt(along * along + 1.0 - dot(center, center))) - center);
        }
        if (projection == 3) {
            float radial = length(ray.xy);
            float angle = atan(radial, -ray.z);
            float squared = angle * angle;
            float radius = angle * (1.0 + squared * (distortion.x + squared *
                (distortion.y + squared * (distortion.z + squared * distortion.w))));
            if (angle > lens.z || lens.w <= 0.0 || radius < 0.0 || radius > lens.w) {
                color = vec4(0.0, 0.0, 0.0, background_alpha);
                return;
            }
            vec2 scale = vec2(min(1.0, 1.0 / aspect), min(1.0, aspect));
            uv = lens.xy + vec2(ray.x, -ray.y) / max(radial, 0.000001)
                * scale * (0.5 * radius / lens.w);
        } else {
            float longitude = atan(ray.x, -ray.z);
            if (projection == 1 && abs(longitude) > PI * 0.5) {
                color = vec4(0.0, 0.0, 0.0, background_alpha);
                return;
            }
            uv = vec2(longitude / (projection == 1 ? PI : 2.0 * PI) + 0.5,
                      0.5 - asin(clamp(ray.y, -1.0, 1.0)) / PI);
        }
    }
    uv += stereo_offset.xy;
    if (projection == 2) uv.x = fract(uv.x);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        color = vec4(0.0, 0.0, 0.0, background_alpha);
        return;
    }
    if (stereo == 1) uv.x = (uv.x + float(video_eye)) * 0.5;
    if (stereo == 2) uv.y = (uv.y + float(video_eye)) * 0.5;
    vec4 sampled = texture(video, crop.xy + uv * crop.zw);
    vec3 rgb = clamp(sampled.rgb, 0.0, 1.0);
    rgb = video_to_sdr(rgb, framing.w, stereo_offset.z, stereo_offset.w);
    if (framing.w == 16.0 || framing.w == 18.0) {

        const float bayer[16] = float[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
        ivec2 pixel = ivec2(gl_FragCoord.xy) & 3;
        float noise = ((bayer[pixel.y * 4 + pixel.x] + 0.5) / 16.0 - 0.5) / 255.0;
        rgb = video_srgb_decode(clamp(video_srgb_encode(rgb) + noise, 0.0, 1.0));
    }
    if (framing.w == -1.0) {
        float alpha = clamp(sampled.a, 0.0, 1.0);
        color = vec4(rgb * alpha, background_alpha == 1.0 ? 1.0 : alpha);
    } else color = vec4(rgb, 1.0);
}
