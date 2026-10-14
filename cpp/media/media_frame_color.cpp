#include "media_internal.h"
extern "C" {
#include <libavutil/mastering_display_metadata.h>
}

media::ColorInfo read_frame_color(AVFrame* frame, Media* media) {
    AVCodecParameters* p = media->file->streams[media->stream]->codecpar;
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(media->software_decoding ? frame->format : p->format));
    media::ColorInfo c{};
    c.transfer = frame->color_trc == AVCOL_TRC_UNSPECIFIED ? p->color_trc : frame->color_trc;
    c.primaries = frame->color_primaries == AVCOL_PRI_UNSPECIFIED ? p->color_primaries : frame->color_primaries;
    c.chroma_location = frame->chroma_location;
    c.bits = desc ? desc->comp[0].depth : 0;
    if((c.primaries == AVCOL_PRI_UNSPECIFIED || c.primaries == 0) && (frame->colorspace == AVCOL_SPC_BT2020_NCL || p->color_space == AVCOL_SPC_BT2020_NCL))
        c.primaries = AVCOL_PRI_BT2020;
    AVFrameSideData* sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    if(sd && sd->size >= sizeof(AVMasteringDisplayMetadata)) {
        const AVMasteringDisplayMetadata* md = reinterpret_cast<const AVMasteringDisplayMetadata*>(sd->data);
        if(md->has_luminance) {
            c.mastering_min_nits = av_q2d(md->min_luminance);
            c.mastering_max_nits = av_q2d(md->max_luminance);
        }
        if(md->has_primaries) {
            for(int i = 0; i < 3; ++i)
                for(int j = 0; j < 2; ++j) c.mastering_xy[i * 2 + j] = av_q2d(md->display_primaries[i][j]);
            c.mastering_xy[6] = av_q2d(md->white_point[0]);
            c.mastering_xy[7] = av_q2d(md->white_point[1]);
        }
    }
    sd = av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
    if(sd && sd->size >= sizeof(AVContentLightMetadata)) {
        const AVContentLightMetadata* cl = reinterpret_cast<const AVContentLightMetadata*>(sd->data);
        c.max_cll = cl->MaxCLL;
        c.max_fall = cl->MaxFALL;
    }
    return c;
}
