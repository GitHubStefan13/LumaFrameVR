#pragma once
#include "pose_math.hpp"
#include <array>
#include <optional>
namespace lumaframevr::ui {
enum class AdvancedKind { settings, rotation };
// The panel's physical size comes from advanced_reference_width, so a smaller texture with the
// same layout stretches every element: 792/609 magnifies the whole panel by 1.3, text included.
// The heights are a little over the strict 1.3 ratio, which buys the extra row spacing.
// The rotation panel shows a header, the two mode buttons, four sliders and nothing else, so
// its height is set by those rows rather than by the 1.3 ratio alone.
// Taller than the strict 1.3 ratio, because the panels draw their text half again as large and
// the rows have to hold it.
inline constexpr std::array<int, 2> settings_size{609, 316}, alignment_size{609, 316};
inline constexpr float advanced_reference_width = 1.6F * (0.6F / 1.4F) * 1.8F;
Pose advanced_pose(AdvancedKind kind, std::array<float, 2> bar_offset, std::array<float, 2> dialog_offset);
class AdvancedDialogs {
   public:
    void open(AdvancedKind kind);
    void toggle(AdvancedKind kind);
    void close();
    bool shown(AdvancedKind kind) const;
    bool grab(AdvancedKind kind, unsigned hand, std::array<float, 2> point);
    bool drag(AdvancedKind kind, unsigned hand, std::array<float, 2> point);
    void release(AdvancedKind kind, unsigned hand);
    std::array<float, 2> offset(AdvancedKind kind) const;

   private:
    struct Dialog {
        bool open{};
        std::array<float, 2> offset{};
        std::optional<std::pair<unsigned, std::array<float, 2>>> grab;
    };
    std::array<Dialog, 2> dialogs_;
};
}  // namespace lumaframevr::ui
