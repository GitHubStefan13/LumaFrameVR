#include "test_registry.hpp"
#include "folder_browser.hpp"
#include "test_assertions.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace std::chrono_literals;
using namespace lumaframevr;
void wait(FolderBrowser& browser) {
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while(browser.loading() && std::chrono::steady_clock::now() < deadline) {
        browser.poll();
        std::this_thread::sleep_for(1ms);
    }
    require(!browser.loading(), "folder scan completes");
    require(browser.message().empty(), "folder scan has no error");
}
int lumaframevr::tests::browser(int, char**) {
    return acceptance_main([] {
        // Prefer TMPDIR, but fall back to the system temp directory so this test
        // runs on a bare checkout instead of skipping.
        const char* scratch = std::getenv("TMPDIR");
        auto root = std::filesystem::path(scratch ? scratch : "/tmp") / ("lumaframevr-browser-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root / "Folder");
        std::ofstream(root / "a.txt") << "not a video";
        std::ofstream(root / "z.mp4") << "invalid video is still listed";
        std::ofstream(root / ".hidden") << "hidden";
        std::filesystem::create_directory_symlink(root / "Folder", root / "Link");
        std::filesystem::create_symlink(root / "missing", root / "Broken");
        auto scanned = scan_folder(root, false, [] { return false; });
        require(scanned.size() == 5, "all files included without decoder filtering");
        require(scanned[0].directory && scanned[1].directory, "directory symlink sorts with folders");
        require(scan_folder(root, true, [] { return false; }).size() == 6, "hidden toggle");
        require(scan_folder(root, false, [] { return true; }).empty(), "cancelled scan stops");
        rejects([&] { scan_folder(root / "missing", false, [] { return false; }); }, "missing folder reports error");
        FolderBrowser browser(root);
        wait(browser);
        require(browser.hidden() && browser.entries().size() == 7, "Hidden enabled and dotfiles included by default");
        browser.toggle_hidden();
        wait(browser);
        require(browser.entries().size() == 6, "browser includes parent plus all non-hidden files");
        const auto a = std::find_if(browser.entries().begin(), browser.entries().end(), [&](auto& e) { return e.path == root / "a.txt"; });
        require(a != browser.entries().end(), "text file retained");
        browser.select(static_cast<std::size_t>(a - browser.entries().begin()));
        browser.toggle_sort();
        require(browser.selected_path() == root / "a.txt", "sort preserves selection by path");
        browser.go(root / "Folder");
        wait(browser);
        require(browser.can_back(), "history records folder");
        browser.back();
        wait(browser);
        require(browser.selected_path() == root / "a.txt", "back restores selection");
        browser.forward();
        wait(browser);
        require(browser.directory() == root / "Folder", "forward restores folder");
        browser.go(root);
        wait(browser);
        require(!browser.can_forward(), "new navigation clears forward stack");
        std::ofstream(root / "new.data") << "refresh";
        browser.refresh();
        wait(browser);
        require(browser.entries().size() == 7, "refresh observes new arbitrary files");
        browser.toggle_hidden();
        wait(browser);
        require(browser.entries().size() == 8, "browser hidden refresh");
        browser.move_selection(-1000);
        require(browser.selected() == 0, "selection clamps low");
        browser.move_selection(1000);
        require(browser.selected() == browser.entries().size() - 1, "selection clamps high");
        browser.go(root / "missing");
        require(!browser.message().empty(), "invalid navigation preserves folder with message");
        require(!browser.places().empty(), "real Places available");
        std::cout << "all-files/hidden/sort/symlink/cancel/history/selection/refresh/Places verified\n";
    });
}
