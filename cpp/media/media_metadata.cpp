#include "media_metadata.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>
extern "C" {
#include <libavutil/spherical.h>
#include <libavutil/stereo3d.h>
}

float media_sample_aspect_ratio(Media* media, AVFrame* frame) {
    AVRational sar = av_guess_sample_aspect_ratio(media->file, media->file->streams[media->stream], frame);
    return sar.num > 0 && sar.den > 0 ? av_q2d(sar) : 1.0;
}

namespace {
int tag_field(AVDictionary* tags, const char* key, bool stereo) {
    if(!tags)
        return -1;
    const AVDictionaryEntry* entry = av_dict_get(tags, key, nullptr, AV_DICT_MATCH_CASE | AV_DICT_IGNORE_SUFFIX);
    if(!entry || !entry->value)
        return -1;
    std::array<char, 64> value{};
    const auto source = std::string_view(entry->value).substr(0, value.size() - 1);
    std::transform(source.begin(), source.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    const std::string_view text(value.data(), source.size());
    auto contains = [&](std::string_view word) { return text.find(word) != std::string_view::npos; };
    if(!stereo) {
        if(contains("equirectangular") || contains("360"))
            return 2;
        if(contains("180"))
            return 1;
        return -1;
    }
    if(contains("mono") || contains("2d"))
        return 0;
    if(contains("side by side") || contains("sbs") || contains("left-right"))
        return 1;
    if(contains("top-bottom") || contains("top bottom") || contains("over-under") || contains("tab") || contains("tb"))
        return 2;
    return -1;
}
}  // namespace

media::Metadata media::read_metadata(const Media& media) {
    Metadata fields{-1, -1, -1};
    const AVCodecParameters* p = media.file->streams[media.stream]->codecpar;
    const AVPacketSideData* data = av_packet_side_data_get(p->coded_side_data, p->nb_coded_side_data, AV_PKT_DATA_STEREO3D);
    if(data && data->size >= sizeof(AVStereo3D)) {
        const auto* s = reinterpret_cast<const AVStereo3D*>(data->data);
        fields[1] = s->type == AV_STEREO3D_2D ? 0 : s->type == AV_STEREO3D_SIDEBYSIDE ? 1 : s->type == AV_STEREO3D_TOPBOTTOM ? 2 : -2;
        if(s->view != AV_STEREO3D_VIEW_PACKED)
            fields[1] = -2;
        fields[2] = fields[1] < 0 ? -2 : fields[1] == 0 ? 0 : !!(s->flags & AV_STEREO3D_FLAG_INVERT);
    }
    int from_tags = tag_field(media.file->streams[media.stream]->metadata, "stereo_mode", true);
    if(from_tags < 0)
        from_tags = tag_field(media.file->metadata, "stereo_mode", true);
    if(from_tags < 0)
        from_tags = tag_field(media.file->metadata, "stereo", true);
    if(from_tags >= 0 && fields[1] < 0) {
        fields[1] = from_tags;
        fields[2] = 0;
    }
    int spherical_tags = tag_field(media.file->metadata, "projection", false);
    if(spherical_tags < 0)
        spherical_tags = tag_field(media.file->streams[media.stream]->metadata, "projection", false);
    if(spherical_tags >= 0 && fields[0] < 0)
        fields[0] = spherical_tags;
    data = av_packet_side_data_get(p->coded_side_data, p->nb_coded_side_data, AV_PKT_DATA_SPHERICAL);
    if(data && data->size >= sizeof(AVSphericalMapping)) {
        const auto* s = reinterpret_cast<const AVSphericalMapping*>(data->data);
        fields[0] = -2;
        if(s->yaw || s->pitch || s->roll)
            return fields;
        if(s->projection == AV_SPHERICAL_EQUIRECTANGULAR)
            fields[0] = 2;
        if(s->projection == AV_SPHERICAL_EQUIRECTANGULAR_TILE && !s->bound_top && !s->bound_bottom) {
            if(!s->bound_left && !s->bound_right)
                fields[0] = 2;
            if(s->bound_left == (1U << 30) && s->bound_right == (1U << 30))
                fields[0] = 1;
        }
    }
    return fields;
}

void media_read_metadata(Media* media) {
    const auto fields = media::read_metadata(*media);
    std::copy(fields.begin(), fields.end(), media->metadata);
}
