#pragma once
#include "media_reader.h"

namespace media {
int packet(Media& media, int audio, AVPacket& packet);
}  // namespace media
