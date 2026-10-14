#include "test_registry.hpp"
#include "media_source.hpp"
#include "test_assertions.hpp"
#include <filesystem>

int lumaframevr::tests::source(int, char**) {
    return acceptance_main([] {
        using lumaframevr::MediaSource;
        for(const auto* url : {"https://HOST/a%20b.mp4?token=A#x", "HTTP://host/a.mp4", "ftp://host/clip.mp4"}) {
            auto source = MediaSource::parse(url).resolved();
            require(source.remote(), "URL remains remote");
            require(source.input() == url, "URL byte identity");
        }
        rejects([] { MediaSource::parse("sftp://host/clip.mp4"); }, "unsupported scheme");
        rejects([] { MediaSource::parse(std::string("bad\0path", 8)); }, "embedded NUL");
        require(MediaSource::parse("host/path").remote() == false, "local is not guessed URL");
        require(MediaSource::parse("https://host/clip.mp4?x=/other").file_name() == "clip.mp4", "query excluded");
        require(MediaSource::parse("https://host").file_name() == "https://host", "empty URL path label");
        auto local = MediaSource::parse(".").resolved();
        require(local.input() == std::filesystem::canonical(".").native(), "canonical local");
        rejects([] { MediaSource::parse("/nonexistent/lumaframevr-source").resolved(); }, "missing local");
    });
}
