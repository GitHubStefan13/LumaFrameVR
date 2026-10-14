#include "media_result.hpp"
#include <algorithm>

int media_error(char* error, size_t capacity, const char* operation, int code) {
    char detail[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, detail, sizeof(detail));
    snprintf(error, capacity, "%s: %s", operation, detail);
    return -1;
}

namespace media {
double Session::duration() const {
    return get()->duration;
}

std::array<int, 3> Session::metadata() const {
    const auto* fields = get()->metadata;
    return {fields[0], fields[1], fields[2]};
}

Result preview(Media* media) {
    if(!media)
        return failure("preview on a closed decoder", AVERROR(EINVAL));
    media->preview_requested = 1;
    return {0, {}};
}

Result Session::preview() {
    return media::preview(get());
}

void stop(Media& media) {
    media_reader_close(&media);
    audio_close(media.audio);
    media.audio = nullptr;
}

void Session::stop() {
    media::stop(*get());
}
}  // namespace media
