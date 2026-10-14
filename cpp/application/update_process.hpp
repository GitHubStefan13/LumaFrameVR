#pragma once
#include "update_state.hpp"
#include <filesystem>

namespace lumaframevr::application {
class UpdateProcess {
   public:
    UpdateProcess() = default;
    ~UpdateProcess();
    UpdateProcess(const UpdateProcess&) = delete;
    UpdateProcess& operator=(const UpdateProcess&) = delete;
    void start(const std::filesystem::path& script, bool install);
    bool poll();
    const ui::UpdateState& state() const { return state_; }

   private:
    void close_process();
    void finish(int status);
    int pid_{-1}, output_{-1};
    ui::UpdateState state_;
    std::string pending_, last_line_;
};
}  // namespace lumaframevr::application
