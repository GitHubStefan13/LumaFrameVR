#include "test_registry.hpp"
#include "controller_pose_filter.hpp"
#include "panel_pointer.hpp"
#include "seek_repeat.hpp"
#include "test_assertions.hpp"
#include <numbers>

using namespace lumaframevr;
Pose pose(float position, float angle) {
    return {{position, -position, position}, {0, 0, std::sin(angle / 2), std::cos(angle / 2)}};
}
float angle(const Pose& p) {
    return 2 * std::atan2(p.rotation[2], p.rotation[3]);
}
std::int64_t time_of(double seconds) {
    return static_cast<std::int64_t>(seconds * 1e9);
}
int lumaframevr::tests::controller(int, char**) {
    return acceptance_main([] {
        for(int rate : {72, 90, 120}) {
            ControllerPoseFilter filter;
            double seconds = 0;
            std::array<double, 4> energy{};
            for(int frame = 0; frame < rate * 3; ++frame) {
                double dt = std::array{1.0, 0.75, 1.25}[frame % 3] / rate;
                seconds += dt;
                float wave = static_cast<float>(std::sin(2 * std::numbers::pi * 10 * seconds));
                auto raw = pose(0.001F * wave, 0.5F * std::numbers::pi_v<float> / 180 * wave);
                auto filtered = filter.update(raw, time_of(seconds)).value();
                if(seconds > 1) {
                    std::array values{raw.position[0], filtered.position[0], angle(raw), angle(filtered)};
                    for(std::size_t i = 0; i < energy.size(); ++i) energy[i] += values[i] * values[i] * dt;
                }
            }
            require(std::sqrt(energy[1] / energy[0]) < 0.6, "variable-rate position tremor");
            require(std::sqrt(energy[3] / energy[2]) < 0.6, "variable-rate rotational tremor");
            filter = {};
            Pose previous = pose(0, 0);
            std::array<float, 2> max_lag{};
            for(int frame = 0; frame <= rate * 2; ++frame) {
                float seconds_f = static_cast<float>(frame) / rate;
                auto raw = pose(2 * std::min(seconds_f, 1.0F), 4 * std::min(seconds_f, 1.0F));
                auto filtered = filter.update(raw, time_of(seconds_f)).value();
                require(filtered.position[0] >= previous.position[0] - 1e-6F, "no position overshoot");
                require(filtered.position[0] <= raw.position[0] + 1e-6F, "position bound");
                require(angle(filtered) >= angle(previous) - 1e-6F, "rotation continuity");
                near(length(filtered.rotation), 1, 1e-6, "unit quaternion");
                if(seconds_f >= 0.5F && seconds_f <= 1) {
                    max_lag[0] = std::max(max_lag[0], (raw.position[0] - filtered.position[0]) / 2);
                    max_lag[1] = std::max(max_lag[1], (angle(raw) - angle(filtered)) / 4);
                }
                previous = filtered;
            }
            require(max_lag[0] < 0.025F && max_lag[1] < 0.025F, "fast motion lag under original 25 ms bound");
            near(previous.position[0], 2, 1e-5, "motion settles position");
            near(angle(previous), 4, 1e-5, "motion settles rotation");
        }
        ControllerPoseFilter filter;
        require(!filter.peek(1), "no pose before advance");
        filter.update(pose(0, 0), 1);
        require(filter.peek(1).has_value() && !filter.peek(2), "peek exact XR time only");
        require(!filter.update(std::nullopt, 2), "missing pose resets");
        near(filter.update(pose(2, 0), 3)->position[0], 2, 1e-8, "fresh pose immediate");
        near(filter.update(pose(3, 0), time_of(1))->position[0], 3, 1e-8, "time gap resets");
        filter = {};
        const float half_turn = 179 * std::numbers::pi_v<float> / 180;
        for(int i = 0; i < 100; ++i) {
            auto raw = pose(0, half_turn);
            if(i % 2)
                for(auto& q : raw.rotation) q = -q;
            near(angle(filter.update(raw, time_of(i / 90.0)).value()), half_turn, 1e-6, "quaternion sign continuity");
        }
        auto crossing = filter.update(pose(0, -half_turn), time_of(100 / 90.0));
        require(angle(*crossing) >= half_turn && angle(*crossing) < 181 * std::numbers::pi_v<float> / 180, "half-turn continuity");
        PanelSurface surface{{{0, 0, -2}, {0, 0, 0, 1}}, {2, 2}};
        auto hit = intersect(ray_of(Pose{}), surface);
        require(hit.has_value(), "forward aim ray hits panel");
        near(hit->distance, 2, 1e-6, "hit distance");
        near(hit->point[1], 0, 1e-6, "local panel center");
        require(!intersect({{3, 0, 0}, {0, 0, -1}}, surface), "off-axis misses");
        require(!intersect({{0, 0, -3}, {0, 0, -1}}, surface), "behind misses");
        std::array surfaces{surface, PanelSurface{{{0, 0, -1}, {0, 0, 0, 1}}, {1, 1}}};
        require(pick(ray_of(Pose{}), surfaces, 8) == 1, "nearest panel wins");
        require(!pick(ray_of(Pose{}), surfaces, 0.5F), "activation range enforced");
        SeekRepeat repeat;
        require(repeat.update(1, 0) == 10, "D-pad tap exactly ten seconds");
        require(repeat.update(0, 0.01) == 0 && repeat.speed() == 0, "release resets");
        require(repeat.update(-1, 0.02) == -10, "opposite tap exactly minus ten");
        require(repeat.update(1, 0.03) == 10 && repeat.speed() == 1, "reverse restarts rate");
        require(repeat.update(1, 5) == 0 && repeat.speed() == 1, "stalled loop does not accrue or ramp");
        for(int i = 1; i <= 200; ++i) repeat.update(1, 5 + i * 0.01);
        require(repeat.speed() == 32, "held seek caps at 32x");
        std::cout << "pose tremor/motion/time/sign/ray/range/seek-repeat acceptance verified\n";
    });
}
