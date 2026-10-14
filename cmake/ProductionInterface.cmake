add_library(lumaframevr_ui cpp/ui/canvas.cpp cpp/ui/rgba_readback.cpp cpp/ui/branding.cpp cpp/ui/branding_assets.cpp cpp/ui/canvas_image.cpp cpp/ui/font_style.cpp cpp/ui/icons.cpp cpp/ui/text.cpp cpp/ui/transport_layout.cpp cpp/ui/transport.cpp cpp/ui/browser_layout.cpp cpp/ui/browser_viewport.cpp cpp/ui/browser_targets.cpp cpp/ui/browser_icons.cpp cpp/ui/browser_chrome.cpp cpp/ui/browser_entries.cpp cpp/ui/browser.cpp cpp/ui/viewer_settings.cpp cpp/ui/update_row.cpp cpp/ui/language_prompt.cpp)
target_include_directories(lumaframevr_ui PUBLIC cpp/ui)
target_link_libraries(lumaframevr_ui PUBLIC lumaframevr_browser lumaframevr_i18n PRIVATE lumaframevr_source)
target_compile_options(lumaframevr_ui PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(lumaframevr_ui_snapshot cpp/diagnostics/ui_snapshot_main.cpp)
target_link_libraries(lumaframevr_ui_snapshot PRIVATE lumaframevr_ui lumaframevr_advanced)
# The settings panels reach into the input maths headers, so the tool needs them on its
# include path; this is what lets a development host render the panels at all.
target_include_directories(lumaframevr_ui_snapshot PRIVATE cpp/input)
target_compile_options(lumaframevr_ui_snapshot PRIVATE -Wall -Wextra -Wpedantic -Werror)
