// The rule that a seek never presents a frame before its target.
//
// A seek deliberately does not land on the target: media_seek.cpp prerolls from
// before it and seeks backwards so the decoder can start at a keyframe. The
// frames decoded between that keyframe and the requested position must then be
// discarded by the frame-yielding code, not shown - for media with audio as
// much as without it, because the audio is already reset to the target and a
// pre-target picture is the wrong frame with the sound on time.
//
// This drives that rule in media_frame.cpp with fixed timestamps, so it needs
// no decoding, no device and no video fixture.
#include "test_registry.hpp"
#include "test_assertions.hpp"
#include "media_session.hpp"
#include "media_owned_frame.hpp"
#include "media_internal.h"

namespace {
// Hand next_frame() a frame at a fixed presentation time without decoding
// anything. A pending preview repeat makes it reuse media->frame instead of
// pulling from the codec, which is the seam this fixture feeds: the timestamps
// are then exactly the ones the test names.
void put_frame(Media* media, int64_t timestamp) {
    av_frame_unref(media->frame);
    media->frame->format = AV_PIX_FMT_YUV420P;
    media->frame->width = 64;
    media->frame->height = 64;
    media->frame->colorspace = AVCOL_SPC_BT709;
    media->frame->color_range = AVCOL_RANGE_MPEG;
    require(av_frame_get_buffer(media->frame, 0) == 0, "allocate a deterministic frame");
    media->frame->best_effort_timestamp = timestamp;
    media->preview_repeat_requested = 1;
}

::media::Session synthetic_session() {
    auto state = std::make_unique<Media>();
    state->references = 1;
    state->file = avformat_alloc_context();
    require(state->file != nullptr, "allocate seek fixture container");
    auto* stream = avformat_new_stream(state->file, nullptr);
    require(stream != nullptr, "allocate seek fixture stream");
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_H264;
    // A real context, because yielding the first frame names the decoder. No
    // packet is ever sent to it; the frames below are handed in directly.
    state->decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_H264));
    require(state->decoder != nullptr && state->decoder->codec != nullptr, "allocate seek fixture decoder");
    state->stream = 0;
    // pts == best_effort_timestamp * time_base - start, so whole-second
    // timestamps land on whole-second presentation times.
    state->time_base = 1.0;
    state->start = 0.0;
    state->step = 1.0 / 30.0;
    state->software_decoding = 1;
    state->seek_target = -1;
    state->frame = av_frame_alloc();
    require(state->frame != nullptr, "allocate seek fixture frame");
    return ::media::Session(state.release());
}
}  // namespace

int lumaframevr::tests::seek_preroll(int, char**) {
    return acceptance_main([] {
        auto session = synthetic_session();
        auto* media = session.get();
        constexpr int64_t target = 5;
        media->seek_target = target;
        media->preview_requested = 1;  // a seek always asks for the first frame as a preview

        // Frames before the target are discarded, not presented, even though the
        // seek requested a preview. Before the fix the first of these came back
        // as a preview frame - the tester's "we drop into the wrong frame".
        for(int64_t timestamp : {target - 2, target - 1}) {
            put_frame(media, timestamp);
            ::media::Frame discarded;
            require(session.next(discarded).status == 2, "a frame before the seek target is discarded");
            require(!discarded.get().owner, "a discarded frame is never presented");
            require(media->preview_requested == 1, "discarding keeps the seek's preview pending");
            require(media->seek_target == target, "discarding does not clear the seek target");
        }

        // The first frame at or after the target is the seek's preview frame:
        // the wrong-frame symptom is gone because the discard ran to the target.
        put_frame(media, target);
        ::media::Frame preview;
        require(session.next(preview).status == 3, "the frame at the target is the seek preview");
        require(preview.get().owner, "the preview frame is presented");
        near(preview.get().pts, double(target), 1e-9, "the preview frame is the target frame");
        require(media->seek_target == target, "the preview keeps the target until the frame settles");
        preview = {};

        // Settling the frame clears the seek target, so playback runs on.
        put_frame(media, target);
        media->preview_requested = 0;
        ::media::Frame settled;
        require(session.next(settled).status == 1, "the settled frame is a picture");
        require(media->seek_target == -1, "reaching the target clears it");
        settled = {};
    });
}
