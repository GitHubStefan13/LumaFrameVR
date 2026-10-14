#include "test_registry.hpp"
#include "remembered_video_settings.hpp"
#include "video_view_parameters.hpp"
#include "test_assertions.hpp"
#include <cstddef>
#include <numbers>

int lumaframevr::tests::presentation(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        RememberedVideoSettings settings;
        auto immersive = settings.open("stereo-VR360-SBS.mp4");
        settings.infer("stereo-VR360-SBS.mp4", {}, immersive);
        require(immersive.projection == Projection::sphere && immersive.stereo == Stereo::side_by_side, "bounded filename hints");
        settings.uploaded("stereo-VR360-SBS.mp4", immersive);
        immersive.lens.centers[1][0] = 0.6F;
        immersive.alignment.yaw = 20;
        settings.edit("stereo-VR360-SBS.mp4", immersive);
        auto plain = settings.open("plain-movie.mp4");
        settings.infer("plain-movie.mp4", {}, plain);
        require(plain.projection == Projection::flat && plain.stereo == Stereo::mono, "unmarked movie is Flat Mono");
        require(plain.alignment.yaw == 0 && plain.lens.centers[1][0] == 0.5F, "calibration never leaks to another video");
        require(settings.open("stereo-VR360-SBS.mp4") == immersive, "complete per-file settings retained");
        auto fresh = settings.open("early.mp4");
        fresh.projection = Projection::flat;
        settings.edit("early.mp4", fresh);
        settings.infer("early.mp4", VideoHints::filename("VR360.mp4"), fresh);
        require(fresh.projection == Projection::flat, "early manual changes block late inference");
        auto conflicts = VideoHints::filename("vr180.vr360.sbs.tb.mp4");
        require(conflicts.projection.conflict && conflicts.stereo.conflict, "conflicting hints stay unknown/conflict");
        require(VideoHints::filename("fisheye.fisheye190.mp4").projection.conflict, "conflicting fisheye FOV is a projection conflict");
        require(VideoHints::native({-2, -2, -2}).projection.conflict, "invalid negative native hint is conflict");
        require(!VideoHints::filename("my360movie.absbssomething.mp4").immersive(), "filename tokens bounded");
        auto half = VideoHints::filename("film-half-sbs.mp4");
        require(half.sbs_format.value == StereoFormat::half, "complete half stereo tokens");
        VideoHints overrides;
        overrides.projection = Hint<Projection>{Projection::flat};
        overrides.stereo = Hint<Stereo>{Stereo::mono};
        RememberedVideoSettings forced(overrides);
        auto current = forced.open("vr360-sbs.mp4");
        forced.infer("vr360-sbs.mp4", {}, current);
        require(current.projection == Projection::flat && current.stereo == Stereo::mono, "explicit overrides win");
        for(auto projection : {Projection::flat, Projection::hemisphere, Projection::sphere, Projection::fisheye}) {
            for(auto stereo : {Stereo::mono, Stereo::side_by_side, Stereo::top_bottom}) {
                for(auto format : {StereoFormat::full, StereoFormat::half}) {
                    VideoPresentation presentation;
                    presentation.projection = projection;
                    presentation.stereo = stereo;
                    presentation.sbs_format = presentation.tb_format = format;
                    presentation.swap_eyes = true;
                    presentation.alignment.stereo_offset = {0.04F, 0.02F};
                    for(int eye = 0; eye < 2; ++eye) {
                        auto parameters = view_parameters(1920, 1080, 1, {}, {-0.7F, 0.7F, -0.7F, 0.7F}, eye, presentation, 0);
                        float expected = 16.0F / 9;
                        if(projection != Projection::flat || format == StereoFormat::full) {
                            if(stereo == Stereo::side_by_side)
                                expected *= 0.5F;
                            if(stereo == Stereo::top_bottom)
                                expected *= 2;
                        }
                        near(parameters.aspect, expected, 1e-6, "stereo full/half aspect per projection");
                        require(parameters.eye == (eye ^ 1), "eye selection swapped");
                        near(parameters.stereo_offset[0], stereo == Stereo::mono ? 0 : (eye - 0.5) * 0.04, 1e-6, "opposite stereo offsets");
                    }
                }
            }
        }
        require(sizeof(VideoViewParameters) == 112 && offsetof(VideoViewParameters, lens) == 80 && offsetof(VideoViewParameters, distortion) == 96, "shader view ABI");
        auto lens = VideoLens{}.parameters(190, 0);
        near(lens[2], 190 * std::numbers::pi / 360, 1e-6, "fisheye half angle");
        near(lens[3], lens[2], 1e-6, "equidistant neutral lens");
        VideoAlignment alignment;
        for(int i = 0; i < 100; ++i) alignment.adjust(AlignmentMode::position, {{{1, 1}, {1, 1}}}, 0.1F, Stereo::mono, -1);
        require(alignment.stereo_offset == std::array<float, 2>{}, "Mono stereo adjustment ignored");
        near(alignment.position[0], -0.5, 1e-6, "position bounded");
        std::cout << "hints/overrides/per-file/settings/Flat-Mono/fisheye/stereo/alignment/ABI verified\n";
    });
}
