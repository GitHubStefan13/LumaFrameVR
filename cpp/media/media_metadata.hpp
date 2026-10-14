#pragma once
#include "media_internal.h"
#include <array>

namespace media {
using Metadata = std::array<int, 3>;
Metadata read_metadata(const Media& media);
}  // namespace media
