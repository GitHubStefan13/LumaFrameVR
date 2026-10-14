#include "test_registry.hpp"
#include "playback_timeline.hpp"
#include "test_assertions.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace std::chrono_literals;
int lumaframevr::tests::timeline(int, char**) {
    return acceptance_main([] {
        using namespace lumaframevr;
        const auto* fixture = std::getenv("LUMAFRAMEVR_TEST_VIDEO");
        if(fixture == nullptr)
            skip("LUMAFRAMEVR_TEST_VIDEO is not set");
        PlaybackTimeline player(MediaSource::parse(fixture), true);
        auto deadline = std::chrono::steady_clock::now() + 8s;
        RetainedFrame held;
        while(!held.valid() && std::chrono::steady_clock::now() < deadline) {
            held = player.poll();
            std::this_thread::sleep_for(1ms);
        }
        require(held.valid(), "first buffered frame presented");
        player.pause(true);
        double paused = player.position();
        std::this_thread::sleep_for(60ms);
        require(!player.poll().valid(), "pause stops presentation");
        near(player.position(), paused, 1e-9, "pause preserves position");
        auto generation = player.seek(0.7);
        require(player.release_required(), "timeline seek requires renderer fence release");
        require(!player.poll().valid(), "retained frame blocks seek until acknowledged");
        held = {};
        player.released(generation);
        deadline = std::chrono::steady_clock::now() + 5s;
        while(!held.valid() && std::chrono::steady_clock::now() < deadline) {
            held = player.poll();
            std::this_thread::sleep_for(1ms);
        }
        require(held.valid() && held.pts() >= 0.7, "paused seek presents exact new still");
        player.suspend(true);
        player.pause(false);
        std::this_thread::sleep_for(40ms);
        require(!player.poll().valid(), "invisible session stays paused independent of manual pause");
        player.suspend(false);
        deadline = std::chrono::steady_clock::now() + 5s;
        while(!player.finished() && std::chrono::steady_clock::now() < deadline) {
            auto next = player.poll();
            if(next.valid())
                held = std::move(next);
            std::this_thread::sleep_for(1ms);
        }
        require(player.finished(), "presentation reaches EOF");
        held = {};
        generation = player.seek(0);
        player.released(generation);
        deadline = std::chrono::steady_clock::now() + 5s;
        while(!held.valid() && std::chrono::steady_clock::now() < deadline) {
            held = player.poll();
            std::this_thread::sleep_for(1ms);
        }
        require(held.valid() && !player.finished(), "presentation restarts from EOF");
        rejects([&] { player.seek(-1); }, "invalid timeline seek");
        std::cout << "buffer/pause/suspend/paused-seek/fence/EOF/restart verified\n";
    });
}
