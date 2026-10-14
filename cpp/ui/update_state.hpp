#pragma once
#include <string>

namespace lumaframevr::ui {
enum class UpdateStatus { never_checked, checking, up_to_date, available, failed, installed };
struct UpdateState {
    UpdateStatus status{UpdateStatus::never_checked};
    bool installing{};
    std::string version;
    std::string message;
};
}  // namespace lumaframevr::ui
