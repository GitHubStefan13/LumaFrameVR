#include "media_internal.h"
extern "C" {
#include <libavutil/hwcontext_drm.h>
}

int validate_software_frame(AVFrame* frame, char* error, size_t capacity) {
    if(frame->decode_error_flags || frame->flags & AV_FRAME_FLAG_CORRUPT)
        return media_error(error, capacity, "Software decoder returned decoding errors", AVERROR_INVALIDDATA);
    int planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(frame->format));
    if(planes < 1 || planes > 3 || !frame->data[0] || !frame->linesize[0])
        return media_error(error, capacity, "Unsupported software frame layout", AVERROR_INVALIDDATA);

    if(planes > 1 && av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format))->log2_chroma_h != 1)
        return media_error(error, capacity, "Unsupported software chroma layout", AVERROR_INVALIDDATA);
    return 0;
}

void fill_software_descriptor(media::FrameData* out, AVFrame* frame) {
    int planes = av_pix_fmt_count_planes(static_cast<AVPixelFormat>(frame->format));
    out->software = 1;
    out->planes = planes;
    out->format = frame->format;
    for(int i = 0; i < planes; ++i) {
        out->data[i] = frame->data[i];
        out->linesize[i] = frame->linesize[i];
    }
}

int validate_dma_buf_frame(AVFrame* frame, char* error, size_t capacity) {
    if(frame->decode_error_flags || frame->flags & AV_FRAME_FLAG_CORRUPT)
        return media_error(error, capacity, "Iris returned decoding errors", AVERROR_INVALIDDATA);
    if(frame->format != AV_PIX_FMT_DRM_PRIME)
        return media_error(error, capacity, "Iris did not return DMA-BUF", AVERROR_INVALIDDATA);
    AVDRMFrameDescriptor* d = reinterpret_cast<AVDRMFrameDescriptor*>(frame->data[0]);
    if(!frame->buf[0] || frame->buf[0]->data != frame->data[0] || frame->buf[0]->size < sizeof(*d) || frame->width <= 0 || frame->height <= 0 ||
       frame->crop_left + frame->crop_right >= (size_t)frame->width || frame->crop_top + frame->crop_bottom >= (size_t)frame->height || d->nb_objects != 1 || d->nb_layers != 1 ||
       d->layers[0].nb_planes != 2 || d->objects[0].fd < 0 || !d->objects[0].size)
        return media_error(error, capacity, "invalid DMA-BUF frame", AVERROR_INVALIDDATA);
    for(int i = 0; i < 2; i++) {
        AVDRMPlaneDescriptor* p = &d->layers[0].planes[i];
        if(p->object_index != 0 || p->offset < 0 || (size_t)p->offset >= d->objects[0].size || p->pitch <= 0)
            return media_error(error, capacity, "invalid DMA-BUF plane", AVERROR_INVALIDDATA);
    }
    return 0;
}

void describe_frame(media::FrameData* out, AVFrame* frame, Media* media) {
    const AVDRMFrameDescriptor* d = reinterpret_cast<const AVDRMFrameDescriptor*>(frame->data[0]);
    const AVDRMLayerDescriptor* layer = &d->layers[0];
    auto& buffer = out->dmabuf;
    buffer = {};
    buffer.fd = d->objects[0].fd;
    buffer.format = layer->format;
    buffer.size = d->objects[0].size;
    buffer.modifier = d->objects[0].format_modifier;
    buffer.pool = media->decoder_generation;
    buffer.source_layout = media::v4l2_source_layout(*frame);
    for(int i = 0; i < 2; ++i) {
        buffer.offsets[i] = layer->planes[i].offset;
        buffer.pitches[i] = layer->planes[i].pitch;
    }
    buffer.width = frame->width;
    buffer.height = frame->height;
    buffer.crop[0] = static_cast<int>(frame->crop_left);
    buffer.crop[1] = static_cast<int>(frame->crop_top);
    buffer.crop[2] = static_cast<int>(frame->crop_right);
    buffer.crop[3] = static_cast<int>(frame->crop_bottom);
    buffer.chroma_location = frame->chroma_location;
    buffer.transfer = frame->color_trc;
    buffer.primaries = frame->color_primaries;
    buffer.stream = media->stream;
}
