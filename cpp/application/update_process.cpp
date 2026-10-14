#include "update_process.hpp"
#include "catalogue.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
namespace lumaframevr::application {
UpdateProcess::~UpdateProcess() {
    close_process();
}
void UpdateProcess::close_process() {
    if(pid_ > 0) {
        kill(-pid_, SIGKILL);
        while(waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
        }
        pid_ = -1;
    }
    if(output_ >= 0) {
        close(output_);
        output_ = -1;
    }
}
void UpdateProcess::start(const std::filesystem::path& script, bool install) {
    if(pid_ > 0)
        return;
    state_ = {ui::UpdateStatus::checking, install, {}, i18n::tr("viewer.update.checking")};
    pending_.clear();
    last_line_.clear();
    int descriptors[2];
    if(pipe2(descriptors, O_CLOEXEC) != 0) {
        state_.status = ui::UpdateStatus::failed;
        state_.message = i18n::tr("viewer.update.start_failed", {{"detail", std::strerror(errno)}});
        return;
    }
    if(fcntl(descriptors[0], F_SETFL, O_NONBLOCK) < 0) {
        const auto detail = std::string(std::strerror(errno));
        close(descriptors[0]);
        close(descriptors[1]);
        state_.status = ui::UpdateStatus::failed;
        state_.message = i18n::tr("viewer.update.start_failed", {{"detail", detail}});
        return;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addclose(&actions, descriptors[0]);
    posix_spawn_file_actions_addclose(&actions, descriptors[1]);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    const auto path = script.string();
    char* arguments[] = {const_cast<char*>("/bin/sh"), const_cast<char*>(path.c_str()), const_cast<char*>(install ? "--install" : "--check"), nullptr};
    pid_t child;
    const int result = posix_spawn(&child, "/bin/sh", &actions, &attributes, arguments, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    close(descriptors[1]);
    if(result != 0) {
        close(descriptors[0]);
        state_.status = ui::UpdateStatus::failed;
        state_.message = i18n::tr("viewer.update.start_failed", {{"detail", std::strerror(result)}});
        return;
    }
    pid_ = child;
    output_ = descriptors[0];
}
void UpdateProcess::finish(int status) {
    state_.message = last_line_.empty() ? i18n::tr("viewer.update.no_output") : last_line_;
    if(WIFEXITED(status) && WEXITSTATUS(status) == 3)
        state_.status = ui::UpdateStatus::up_to_date;
    else if(WIFEXITED(status) && WEXITSTATUS(status) == 0 && state_.installing)
        state_.status = ui::UpdateStatus::installed;
    else if(WIFEXITED(status) && WEXITSTATUS(status) == 0 && last_line_.starts_with("AVAILABLE\t")) {
        const auto separator = last_line_.find('\t', 10);
        if(separator == std::string::npos || separator == 10) {
            state_.status = ui::UpdateStatus::failed;
            state_.message = i18n::tr("viewer.update.invalid_output");
        } else {
            state_.status = ui::UpdateStatus::available;
            state_.version = last_line_.substr(10, separator - 10);
            state_.message = last_line_.substr(separator + 1);
        }
    } else {
        state_.status = ui::UpdateStatus::failed;
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
            state_.message = i18n::tr("viewer.update.invalid_output");
    }
}
bool UpdateProcess::poll() {
    if(pid_ <= 0)
        return false;
    const auto before = state_.message;
    char buffer[4096];
    bool drained = false;
    for(int batch = 0; batch < 16; ++batch) {
        const auto count = read(output_, buffer, sizeof(buffer));
        if(count <= 0) {
            if(count < 0 && errno == EINTR)
                continue;
            drained = count == 0;
            break;
        }
        pending_.append(buffer, static_cast<std::size_t>(count));
        std::size_t end;
        while((end = pending_.find('\n')) != std::string::npos) {
            last_line_ = pending_.substr(0, end);
            pending_.erase(0, end + 1);
            state_.message = last_line_;
        }
        if(pending_.size() > 65536 || last_line_.size() > 65536) {
            close_process();
            state_.status = ui::UpdateStatus::failed;
            state_.message = i18n::tr("viewer.update.too_long");
            return true;
        }
    }
    int status;
    if(drained && waitpid(pid_, &status, WNOHANG) == pid_) {
        pid_ = -1;
        close(output_);
        output_ = -1;
        if(!pending_.empty())
            last_line_ = pending_;
        pending_.clear();
        finish(status);
        return true;
    }
    return before != state_.message;
}
}  // namespace lumaframevr::application
