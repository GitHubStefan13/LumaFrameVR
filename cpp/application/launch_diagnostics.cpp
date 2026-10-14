#include "launch_diagnostics.hpp"
#include <csignal>
#include <execinfo.h>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ucontext.h>
#include <unistd.h>
namespace lumaframevr::application {
namespace {
alignas(16) unsigned char alternate_stack[65536];
char* literal(char* output, const char* text) {
    while(*text) *output++ = *text++;
    return output;
}
char* hexadecimal(char* output, std::uintptr_t value) {
    output = literal(output, "0x");
    for(int digit = static_cast<int>(sizeof(value) * 2) - 1; digit >= 0; --digit) *output++ = "0123456789abcdef"[(value >> (digit * 4)) & 15];
    return output;
}
void fatal(int signal, siginfo_t* info, void* context) {
    std::uintptr_t pc{};
    if(context) {
#if defined(__aarch64__)
        pc = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
#elif defined(__x86_64__)
        pc = static_cast<std::uintptr_t>(static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP]);
#endif
    }
    char bytes[256];
    char* output = literal(bytes, "\n[LumaFrameVR fatal] signal=");
    if(signal >= 10)
        *output++ = static_cast<char>('0' + signal / 10);
    *output++ = static_cast<char>('0' + signal % 10);
    output = literal(output, " pc=");
    output = hexadecimal(output, pc);
    output = literal(output, " fault=");
    output = hexadecimal(output, info ? reinterpret_cast<std::uintptr_t>(info->si_addr) : 0);
    output = literal(output, "\n[LumaFrameVR fatal end]\n");
    const auto ignored = write(STDERR_FILENO, bytes, static_cast<std::size_t>(output - bytes));
    static_cast<void>(ignored);
    // The address alone is useless: every shipped library is stripped, so the offset cannot
    // be resolved afterwards. A backtrace names the library and the offset at the moment of
    // the crash, which is what makes the next report diagnosable.
    void* frames[32];
    const int count = backtrace(frames, 32);
    static const char header[] = "[LumaFrameVR stack]\n";
    const auto stack = write(STDERR_FILENO, header, sizeof(header) - 1);
    static_cast<void>(stack);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    kill(getpid(), signal);
}
}  // namespace
void install_launch_diagnostics() {
    stack_t existing{};
    if(sigaltstack(nullptr, &existing) == 0 && (existing.ss_flags & SS_DISABLE)) {
        stack_t stack{};
        stack.ss_sp = alternate_stack;
        stack.ss_size = sizeof(alternate_stack);
        if(sigaltstack(&stack, nullptr))
            std::cerr << "Fatal diagnostic alternate stack unavailable\n";
    }
    for(const int signal : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) {
        struct sigaction previous{}, action{};
        if(sigaction(signal, nullptr, &previous) || previous.sa_handler != SIG_DFL)
            continue;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
        action.sa_sigaction = fatal;
        if(sigaction(signal, &action, nullptr))
            std::cerr << "Fatal diagnostic handler unavailable for signal " << signal << '\n';
    }
}
void log_launch_environment(std::ostream& output) {
    output << "[LumaFrameVR launch environment]\n";
    std::error_code error;
    output << "working_directory=" << std::quoted(std::filesystem::current_path(error).string()) << '\n';
    for(const auto* name : {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "XDG_RUNTIME_DIR", "XR_RUNTIME_JSON", "VR_PATHREG_OVERRIDE", "VR_LOG_PATH", "LD_LIBRARY_PATH", "LD_PRELOAD",
                            "VK_ICD_FILENAMES", "VK_DRIVER_FILES", "ENABLE_GAMESCOPE_WSI", "DISABLE_GAMESCOPE_WSI", "DISPLAY", "WAYLAND_DISPLAY", "LUMAFRAMEVR_LOG_FILE"}) {
        const auto* value = std::getenv(name);
        output << name << '=' << std::quoted(value ? value : "unset") << '\n';
    }
    for(const auto* name : {"PULSE_SERVER", "PULSE_COOKIE", "DBUS_SESSION_BUS_ADDRESS"}) output << name << '=' << (std::getenv(name) ? "set" : "unset") << '\n';
    output << "[LumaFrameVR launch environment end]\n" << std::flush;
}
}  // namespace lumaframevr::application
