#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "media_session.hpp"
#include "media_metadata.hpp"
#include <algorithm>
extern "C" {
#include <libavutil/spherical.h>
#include <libavutil/stereo3d.h>
}

int lumaframevr::tests::presentation_native(int, char**) {
    return acceptance_main([] {
        auto state = std::make_unique<Media>();
        state->references = 1;
        state->file = avformat_alloc_context();
        require(state->file != nullptr, "allocate metadata container");
        ::media::Session session(state.release());
        auto* m = session.get();
        m->stream = 1;
        require(avformat_new_stream(m->file, nullptr) != nullptr, "allocate unrelated stream");
        auto* stream = avformat_new_stream(m->file, nullptr);
        require(stream != nullptr, "allocate selected video stream");
        AVFrame frame{};
        frame.sample_aspect_ratio = {64, 45};
        near(media_sample_aspect_ratio(m, &frame), 64.0 / 45, 1e-6, "frame SAR fallback");
        stream->sample_aspect_ratio = {16, 15};
        near(media_sample_aspect_ratio(m, &frame), 16.0 / 15, 1e-6, "stream SAR overrides frame");
        stream->sample_aspect_ratio = {0, 1};
        for(auto sar : {AVRational{-1, 1}, AVRational{0, 1}, AVRational{1, 0}}) {
            frame.sample_aspect_ratio = sar;
            require(media_sample_aspect_ratio(m, &frame) == 1.0f, "invalid SAR defaults to square pixels");
        }
        auto expect = [&](int projection, int stereo, int swap) {
            const ::media::Metadata expected{projection, stereo, swap};
            const auto actual = ::media::read_metadata(*m);
            require(actual == expected, "typed metadata matches projection/stereo/swap");
            std::copy(actual.begin(), actual.end(), m->metadata);
            require(session.metadata() == expected, "Session returns metadata values");
        };
        expect(-1, -1, -1);
        auto* p = stream->codecpar;
        auto* stereo = av_stereo3d_alloc();
        require(stereo != nullptr, "allocate stereo side data");
        require(av_packet_side_data_add(&p->coded_side_data, &p->nb_coded_side_data, AV_PKT_DATA_STEREO3D,
                                        stereo, sizeof(*stereo), 0) != nullptr, "attach stereo side data");
        for(int type = 0; type < 3; ++type) {
            stereo->type = type == 0 ? AV_STEREO3D_2D : type == 1 ? AV_STEREO3D_SIDEBYSIDE : AV_STEREO3D_TOPBOTTOM;
            stereo->flags = AV_STEREO3D_FLAG_INVERT;
            expect(-1, type, type != 0);
        }
        stereo->flags = 0;
        expect(-1, 2, 0);
        stereo->type = AV_STEREO3D_CHECKERBOARD;
        expect(-1, -2, -2);
        stereo->type = AV_STEREO3D_SIDEBYSIDE;
        stereo->view = AV_STEREO3D_VIEW_LEFT;
        expect(-1, -2, -2);
        stereo->view = AV_STEREO3D_VIEW_PACKED;
        size_t size{};
        auto* sphere = av_spherical_alloc(&size);
        require(sphere != nullptr, "allocate spherical side data");
        sphere->projection = AV_SPHERICAL_EQUIRECTANGULAR;
        require(av_packet_side_data_add(&p->coded_side_data, &p->nb_coded_side_data, AV_PKT_DATA_SPHERICAL,
                                        sphere, size, 0) != nullptr, "attach spherical side data");
        expect(2, 1, 0);
        sphere->projection = AV_SPHERICAL_EQUIRECTANGULAR_TILE;
        sphere->bound_left = sphere->bound_right = 1U << 30;
        expect(1, 1, 0);
        sphere->bound_top = 1;
        expect(-2, 1, 0);
        sphere->projection = AV_SPHERICAL_CUBEMAP;
        expect(-2, 1, 0);
        sphere->projection = AV_SPHERICAL_EQUIRECTANGULAR;
        sphere->pitch = 65536;
        expect(-2, 1, 0);
        m->stream = 0;
        expect(-1, -1, -1);
    });
}
