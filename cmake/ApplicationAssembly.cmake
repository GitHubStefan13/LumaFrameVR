add_library(lumaframevr_application_controller
  cpp/application/preview_layout.cpp
  cpp/application/playback_lifetime.cpp
  cpp/application/image_viewing.cpp
  cpp/application/controller_images.cpp
  cpp/application/browser_previews.cpp
  cpp/application/controller_snapshots.cpp
  cpp/application/update_process.cpp
  cpp/application/controller_viewer_settings.cpp
  cpp/application/controller_lifecycle.cpp
  cpp/application/controller_transport.cpp
  cpp/application/controller_activation.cpp
  cpp/application/controller_pointer.cpp
  cpp/application/controller_list_drag.cpp
  cpp/application/controller_navigation.cpp
  cpp/application/controller_previews.cpp
  cpp/application/scene_placement.cpp
  cpp/application/scene_pointer.cpp
  cpp/application/scene_raster.cpp)
target_include_directories(lumaframevr_application_controller PUBLIC cpp/application)
target_link_libraries(lumaframevr_application_controller PUBLIC lumaframevr_images lumaframevr_diagnostics lumaframevr_thumbnails lumaframevr_controller_math lumaframevr_options lumaframevr_playback)
target_compile_options(lumaframevr_application_controller PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(lumaframevr_application_controller PRIVATE LUMAFRAMEVR_VERSION="${PROJECT_VERSION}")
