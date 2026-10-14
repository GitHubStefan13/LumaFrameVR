set(LUMAFRAMEVR_ADDITIONAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cpp")
set(LUMAFRAMEVR_ACTIVE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(Threads REQUIRED)
foreach(required lumaframevr_ui lumaframevr_presentation lumaframevr_media lumaframevr_native)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "Additional components require existing target ${required}")
  endif()
endforeach()
add_library(lumaframevr_advanced
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/settings_fields.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/settings_buttons.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/settings_numeric.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/advanced_layout.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/alignment_controls.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/advanced_controller.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/advanced_sticks.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui/advanced_dialog_state.cpp)
target_include_directories(lumaframevr_advanced PUBLIC "${LUMAFRAMEVR_ADDITIONAL_ROOT}/ui")
target_link_libraries(lumaframevr_advanced PUBLIC lumaframevr_ui lumaframevr_presentation)
add_library(lumaframevr_thumbnails
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/preview_fetcher.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/thumbnail_pixels.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/thumbnail_cache.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/thumbnail_persistence.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/thumbnail_disk_store.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/thumbnail_cache_worker.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview/seek_preview_readout.cpp)
target_include_directories(lumaframevr_thumbnails PUBLIC "${LUMAFRAMEVR_ADDITIONAL_ROOT}/preview")
target_link_libraries(lumaframevr_thumbnails PUBLIC lumaframevr_media lumaframevr_ui lumaframevr_persistent_files PRIVATE Threads::Threads)
add_library(lumaframevr_diagnostics
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/statistics.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/metrics_parse.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/device_metrics.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/telemetry.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/information_panel.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/profile_csv.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/profile_measure.cpp
  ${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics/native_still_profile.cpp)
target_include_directories(lumaframevr_diagnostics PUBLIC "${LUMAFRAMEVR_ADDITIONAL_ROOT}/diagnostics")
target_link_libraries(lumaframevr_diagnostics PUBLIC lumaframevr_advanced lumaframevr_media PRIVATE lumaframevr_native Threads::Threads)
foreach(component lumaframevr_advanced lumaframevr_thumbnails lumaframevr_diagnostics)
  target_compile_features(${component} PUBLIC cxx_std_20)
  target_compile_options(${component} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()
option(LUMAFRAMEVR_ADDITIONAL_TESTS "Build additional component acceptance tests" OFF)
set(LUMAFRAMEVR_ADDITIONAL_TEST_VIDEO "" CACHE FILEPATH "Fixture video for advanced/preview/diagnostics tests")
