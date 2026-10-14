#pragma once
#include "media_session.hpp"
#include "media_internal.h"
#include <vector>

namespace media {
Result receive(Media& media);
Result reconfigure(Media& media, bool released);
void stop(Media& media);
void unref(Media* media) noexcept;
inline Result failure(const char* operation, int code) {
    char detail[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, detail, sizeof(detail));
    return {-1, std::string(operation) + ": " + detail};
}
template <class Function>
std::string legacy_error(Function&& function, size_t extra = 0) {
    std::vector<char> buffer(4096 + extra);
    function(buffer.data(), buffer.size());
    return buffer.data();
}
inline int export_result(const Result& result, char* error, size_t capacity) {
    if(result.status < 0)
        snprintf(error, capacity, "%s", result.error.c_str());
    return result.status;
}
}  // namespace media
