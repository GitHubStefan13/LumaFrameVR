#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lumaframevr {
struct FolderEntry {
    std::filesystem::path path;
    bool directory;
};
using Place = std::pair<std::string, std::filesystem::path>;
std::vector<FolderEntry> scan_folder(const std::filesystem::path& directory, bool hidden, const std::function<bool()>& cancelled);
std::vector<Place> browser_places();
class FolderBrowser {
   public:
    explicit FolderBrowser(std::filesystem::path directory);
    ~FolderBrowser();
    FolderBrowser(const FolderBrowser&) = delete;
    FolderBrowser& operator=(const FolderBrowser&) = delete;
    const std::filesystem::path& directory() const { return directory_; }
    const std::vector<FolderEntry>& entries() const { return entries_; }
    std::size_t selected() const { return selected_; }
    std::filesystem::path selected_path() const;
    const std::string& message() const { return message_; }
    const std::vector<Place>& places() const { return places_; }
    bool loading() const { return loading_; }
    bool hidden() const { return hidden_; }
    bool descending() const { return descending_; }
    bool poll();
    bool select(std::size_t index);
    void move_selection(std::ptrdiff_t direction);
    void go(const std::filesystem::path& path);
    void parent();
    void back();
    void forward();
    bool can_back() const { return !back_.empty(); }
    bool can_forward() const { return !forward_.empty(); }
    void refresh();
    void toggle_hidden();
    void toggle_sort();
    std::optional<std::filesystem::path> open();

   private:
    struct ScanState;
    static void scan_loop(const std::shared_ptr<ScanState>& state);
    void load(const std::filesystem::path& path);
    void sort();
    void remember();
    static void push_history(std::vector<std::filesystem::path>& history, const std::filesystem::path& path);
    std::filesystem::path directory_;
    std::vector<FolderEntry> entries_;
    std::size_t selected_{};
    std::string message_;
    bool hidden_{true}, descending_{}, loading_{};
    std::vector<std::filesystem::path> back_, forward_;
    std::map<std::filesystem::path, std::filesystem::path> selections_;
    std::vector<Place> places_;
    std::shared_ptr<ScanState> scan_;
    std::thread thread_;
};
}  // namespace lumaframevr
