#include "canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace lumaframevr::ui {
std::u32string decode_utf8(std::string_view text) {
    std::u32string result;
    for(std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index++]);
        if(first < 0x80) {
            result.push_back(first);
            continue;
        }
        int count = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : first >= 0xc0 ? 1 : 0;
        char32_t character = first & (count == 3 ? 7 : count == 2 ? 15 : 31);
        bool valid = count != 0;
        for(int next = 0; next < count; ++next) {
            if(index == text.size() || (static_cast<unsigned char>(text[index]) & 0xc0) != 0x80) {
                valid = false;
                break;
            }
            character = (character << 6) | (static_cast<unsigned char>(text[index++]) & 0x3f);
        }
        result.push_back(valid ? character : U'\ufffd');
    }
    return result;
}
std::string encode_utf8(std::u32string_view text) {
    std::string result;
    for(const auto character : text) {
        if(character < 0x80)
            result.push_back(static_cast<char>(character));
        else if(character < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (character >> 6)));
            result.push_back(static_cast<char>(0x80 | (character & 0x3f)));
        } else if(character < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (character >> 12)));
            result.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (character & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (character >> 18)));
            result.push_back(static_cast<char>(0x80 | ((character >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (character & 0x3f)));
        }
    }
    return result;
}
std::string fit_label(std::string_view text, std::size_t columns, bool preserve_extension) {
    auto characters = decode_utf8(text);
    if(characters.size() <= columns)
        return std::string(text);
    if(preserve_extension) {
        const auto dot = characters.rfind(U'.');
        if(dot != std::u32string::npos && dot > 0) {
            const auto tail = characters.substr(dot);
            const auto budget = columns > tail.size() + 1 ? columns - tail.size() - 1 : 0;
            if(budget > tail.size())
                return encode_utf8(characters.substr(0, budget) + U"\u2026" + tail);
        }
    }
    characters.resize(columns ? columns - 1 : 0);
    return encode_utf8(characters + U"\u2026");
}
std::string escape_path(const std::filesystem::path& path) {
    std::string result;
    for(const auto character : path.native()) {
        const auto byte = static_cast<unsigned char>(character);
        if((byte >= 33 && byte <= 126 && byte != 92) || byte == 32)
            result.push_back(character);
        else {
            char escaped[5]{};
            std::snprintf(escaped, sizeof(escaped), "\\x%02x", byte);
            result += escaped;
        }
    }
    return result;
}
std::string timestamp(double seconds) {
    if(!std::isfinite(seconds) || seconds < 0)
        return "--:--";
    const auto total = seconds >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()) ? std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(seconds);
    char text[48]{};
    if(total >= 3600)
        std::snprintf(text, sizeof(text), "%llu:%02llu:%02llu", static_cast<unsigned long long>(total / 3600), static_cast<unsigned long long>(total / 60 % 60),
                      static_cast<unsigned long long>(total % 60));
    else
        std::snprintf(text, sizeof(text), "%02llu:%02llu", static_cast<unsigned long long>(total / 60), static_cast<unsigned long long>(total % 60));
    return text;
}
}  // namespace lumaframevr::ui
