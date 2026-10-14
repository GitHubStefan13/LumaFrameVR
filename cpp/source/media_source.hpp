#pragma once
#include <string>
#include <string_view>

namespace lumaframevr {
class MediaSource {
   public:
    static MediaSource parse(std::string_view argument);
    MediaSource resolved() const;
    bool remote() const noexcept { return remote_; }
    const std::string& input() const noexcept { return input_; }
    std::string file_name() const;

   private:
    MediaSource(std::string input, bool remote) : input_(std::move(input)), remote_(remote) {}
    std::string input_;
    bool remote_;
};
}  // namespace lumaframevr
