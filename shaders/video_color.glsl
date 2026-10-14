// Modified fork colour stage (2026-10-06 UTC).
// Equations: SMPTE ST 2084 / ITU-R BT.2100 (PQ, HLG), Reinhard et al.
// "Photographic Tone Reproduction for Digital Images" (2002).
// Output: linear BT.709, encoded by the existing sRGB colour attachment.
// HDR is mapped to a nominal 100-nit SDR target; this is NOT native HDR output.
vec3 video_srgb_decode(vec3 rgb) {
    return mix(rgb / 12.92, pow((rgb + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), rgb));
}
vec3 video_srgb_encode(vec3 rgb) {
    return mix(12.92 * rgb, 1.055 * pow(rgb, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), rgb));
}
vec3 video_to_sdr(vec3 rgb, float transfer, float primaries, float peak) {
    rgb = clamp(rgb, 0.0, 1.0);
    bool hdr = transfer == 16.0 || transfer == 18.0;
    vec3 weights = primaries == 9.0 ? vec3(0.2627, 0.6780, 0.0593) : vec3(0.2126, 0.7152, 0.0722);
    if (transfer == 16.0) {
        // ST 2084 EOTF, absolute luminance in cd/m^2.
        vec3 p = pow(rgb, vec3(1.0 / (2523.0 / 32.0)));
        rgb = 10000.0 * pow(max(p - 3424.0 / 4096.0, vec3(0.0)) /
            max(2413.0 / 128.0 - (2392.0 / 128.0) * p, vec3(1e-6)), vec3(1.0 / (2610.0 / 16384.0)));
    } else if (transfer == 18.0) {
        // BT.2100 inverse HLG OETF followed by luminance-dependent OOTF.
        rgb = mix(rgb * rgb / 3.0, (exp((rgb - 0.55991073) / 0.17883277) + 0.28466892) / 12.0,
                  step(vec3(0.5), rgb));
        float gamma = 1.2 + 0.42 * log(max(peak, 100.0) / 1000.0) / log(10.0);
        rgb *= peak * pow(max(dot(rgb, weights), 1e-8), gamma - 1.0);
    } else if (transfer == 1.0 || transfer == 6.0 || transfer == 14.0 || transfer == 15.0) {
        // BT.709 / BT.2020 SDR inverse OETF.
        rgb = mix(rgb / 4.5, pow((rgb + 0.099) / 1.099, vec3(1.0 / 0.45)), step(vec3(0.081), rgb));
    } else if (transfer == 4.0 || transfer == 5.0) {
        rgb = pow(rgb, vec3(transfer == 4.0 ? 2.2 : 2.8));
    } else {
        // sRGB (13), or the retained default for unlabelled SDR (0/2).
        rgb = video_srgb_decode(rgb);
    }
    if (hdr) {
        rgb /= 100.0;
        float luma = max(dot(rgb, weights), 0.0);
        float white = max(peak / 100.0, 1.0);
        float mapped = luma * (1.0 + luma / (white * white)) / (1.0 + luma);
        rgb *= luma > 1e-8 ? mapped / luma : 0.0;
    }
    if (primaries == 9.0) {
        // Linear D65 BT.2020 -> BT.709. Out-of-gamut colours are clipped below.
        rgb = mat3(1.660491, -0.124550, -0.018151,
                   -0.587641, 1.132900, -0.100579,
                   -0.072850, -0.008349, 1.118730) * rgb;
    }
    return clamp(rgb, 0.0, 1.0);
}
