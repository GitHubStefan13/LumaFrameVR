# Test registration.
#
# All acceptance tests build into one binary, lumaframevr_tests, and are
# selected by name: `lumaframevr_tests <test> [arguments...]`. This keeps the
# build graph small (one link step instead of one per test) while every test
# still runs as its own CTest case with its own arguments and timeout.
#
# Included last, after every library the tests link against exists.

if(NOT BUILD_TESTING)
  return()
endif()

set(LUMAFRAMEVR_ASSEMBLY_VIDEO "" CACHE FILEPATH "Real application fixture for the media loop test")

add_executable(lumaframevr_tests
  cpp/tests/test_main.cpp
  cpp/tests/v4l2_acceptance.cpp
  cpp/tests/v4l2_queue_acceptance.cpp
  cpp/tests/v4l2_device_acceptance.cpp
  cpp/tests/application_options_acceptance.cpp
  cpp/tests/browser_acceptance.cpp
  cpp/tests/browser_viewport_acceptance.cpp
  cpp/tests/catalogue_acceptance.cpp
  cpp/tests/controller_acceptance.cpp
  cpp/tests/controller_bindings_acceptance.cpp
  cpp/tests/dma_import_acceptance.cpp
  cpp/tests/gpu_color_acceptance.cpp
  cpp/tests/image_loader_acceptance.cpp
  cpp/tests/image_reader_acceptance.cpp
  cpp/tests/localization_acceptance.cpp
  cpp/tests/media_acceptance.cpp
  cpp/tests/media_errors.cpp
  cpp/tests/audio_native.cpp
  cpp/tests/audio_operations.cpp
  cpp/tests/packet_reader.cpp
  cpp/tests/packet_limits.cpp
  cpp/tests/seek_preroll.cpp
  cpp/tests/presentation_native.cpp
  cpp/tests/native_audio_acceptance.cpp
  cpp/tests/media_loop_acceptance.cpp
  cpp/tests/presentation_acceptance.cpp
  cpp/tests/routing_acceptance.cpp
  cpp/tests/shader_module_acceptance.cpp
  cpp/tests/source_acceptance.cpp
  cpp/tests/timeline_acceptance.cpp
  cpp/tests/ui_contract.cpp)
target_include_directories(lumaframevr_tests PRIVATE cpp/tests cpp/application)
target_link_libraries(lumaframevr_tests PRIVATE
  lumaframevr_application_controller
  lumaframevr_openxr
  lumaframevr_vulkan
  lumaframevr_playback
  lumaframevr_media
  lumaframevr_native
  lumaframevr_images
  lumaframevr_source
  lumaframevr_browser
  lumaframevr_i18n
  lumaframevr_ui
  lumaframevr_controller_math
  lumaframevr_presentation
  lumaframevr_options)
target_compile_options(lumaframevr_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
# Test-only I/O boundaries gate reader calls and simulate PulseAudio operations.
# Wrappers delegate to the real functions whenever their fixture is inactive.
target_link_options(lumaframevr_tests PRIVATE
  "LINKER:--wrap=av_read_frame"
  "LINKER:--wrap=output_open"
  "LINKER:--wrap=pa_mainloop_iterate"
  "LINKER:--wrap=pa_context_get_state"
  "LINKER:--wrap=pa_stream_get_state"
  "LINKER:--wrap=pa_operation_get_state"
  "LINKER:--wrap=pa_operation_cancel"
  "LINKER:--wrap=pa_operation_unref"
  "LINKER:--wrap=pa_stream_cork"
  "LINKER:--wrap=pa_stream_flush"
  "LINKER:--wrap=pa_stream_update_timing_info"
  "LINKER:--wrap=pa_stream_drain"
  "LINKER:--wrap=pa_stream_writable_size"
  "LINKER:--wrap=pa_stream_write"
  "LINKER:--wrap=pa_stream_get_latency"
  "LINKER:--wrap=pa_stream_disconnect"
  "LINKER:--wrap=pa_stream_unref"
  "LINKER:--wrap=pa_stream_get_context"
  "LINKER:--wrap=pa_context_errno"
  "LINKER:--wrap=pa_stream_get_device_name")
target_compile_definitions(lumaframevr_tests PRIVATE LUMAFRAMEVR_SHADER_DIRECTORY="${LUMAFRAMEVR_SHADER_DIRECTORY}")
add_dependencies(lumaframevr_tests lumaframevr_shaders)

# name|arguments|timeout|skippable  (fields separated by | so the list separator ; is free)
# skippable=1 means the test exits 77 when its optional fixture is absent, which
# CTest reports as a skip rather than a failure.
set(LUMAFRAMEVR_TESTS
  "application_options||30|0"
  "browser||30|0"
  "browser_viewport|${CMAKE_BINARY_DIR}/viewport-fixtures|45|0"
  "catalogue|${CMAKE_CURRENT_SOURCE_DIR}/cpp/i18n/lang ${CMAKE_BINARY_DIR}/catalogue-fixtures|30|0"
  "controller||30|0"
  "controller_bindings||30|0"
  "dma_import||30|0"
  "gpu_color||60|0"
  "image_loader||15|0"
  "image_reader|${LUMAFRAMEVR_IMAGE_FIXTURES}|45|1"
  "localization|${CMAKE_CURRENT_SOURCE_DIR}/cpp/i18n/lang ${CMAKE_BINARY_DIR}/localization-fixtures|45|0"
  "media||90|1"
  "media_errors||30|0"
  "audio_native|${CMAKE_BINARY_DIR}/audio-native-fixtures|45|0"
  "audio_operations||30|0"
  "packet_reader||45|1"
  "packet_limits||30|0"
  "seek_preroll||30|0"
  "presentation_native||30|0"
  "v4l2_formats||30|0"
  "v4l2_layout||30|0"
  "v4l2_queue||30|0"
  "v4l2_device||60|1"
  "v4l2_bitstream||30|1"
  "media_loop|${LUMAFRAMEVR_ASSEMBLY_VIDEO}|45|1"
  "presentation||30|0"
  "routing|${CMAKE_BINARY_DIR}/fixtures|45|0"
  "shader_module|${CMAKE_BINARY_DIR}/shader-module-fixtures|30|0"
  "source||30|0"
  "timeline||30|1"
  "ui_contract||30|0")

foreach(entry IN LISTS LUMAFRAMEVR_TESTS)
  string(REPLACE "|" ";" fields "${entry}")
  list(GET fields 0 name)
  list(GET fields 1 arguments)
  list(GET fields 2 timeout)
  list(GET fields 3 skippable)
  separate_arguments(arguments UNIX_COMMAND "${arguments}")
  add_test(NAME ${name} COMMAND lumaframevr_tests ${name} ${arguments})
  set_tests_properties(${name} PROPERTIES TIMEOUT ${timeout})
  if(skippable)
    set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endforeach()

# Tests that are not part of the merged binary: they drive the real player or
# the Python tooling.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(lumaframevr_updater_tests cpp/tests/updater_acceptance.cpp)
target_link_libraries(lumaframevr_updater_tests PRIVATE lumaframevr_application_controller)
target_compile_options(lumaframevr_updater_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME updater_ui_acceptance COMMAND lumaframevr_updater_tests "${CMAKE_BINARY_DIR}/updater-fixtures")
set_tests_properties(updater_ui_acceptance PROPERTIES TIMEOUT 30)
add_test(NAME updater_logic_acceptance COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s tests -p "test_updat*.py" -v)
set_tests_properties(updater_logic_acceptance PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 30 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
add_test(NAME installer_acceptance COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s installer/tests -p "test_install_sh.py" -v)
set_tests_properties(installer_acceptance PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 300 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
if(CMAKE_CROSSCOMPILING_EMULATOR)
  # The launcher test drives the real player through launch.sh, so a cross build
  # runs it through the emulator ctest already uses for the native tests. On a
  # host build this stays empty and the test runs the player directly.
  string(REPLACE ";" " " LUMAFRAMEVR_LAUNCH_TEST_RUNNER "${CMAKE_CROSSCOMPILING_EMULATOR}")
endif()
add_test(NAME launcher_acceptance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/test_launcher.py" "$<TARGET_FILE:lumaframevr>" "${CMAKE_CURRENT_BINARY_DIR}/launcher-fixtures" ${LUMAFRAMEVR_LAUNCH_TEST_RUNNER})
add_test(NAME application_cli_help COMMAND lumaframevr --help)
add_test(NAME application_cli_version COMMAND lumaframevr --version)
if(LUMAFRAMEVR_ASSEMBLY_VIDEO)
  add_test(NAME application_cli_probe COMMAND lumaframevr "${LUMAFRAMEVR_ASSEMBLY_VIDEO}" --probe)
  set_tests_properties(application_cli_probe PROPERTIES TIMEOUT 45)
endif()
add_test(NAME launch_diagnostics_acceptance COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/test_launch_diagnostics.py" "$<TARGET_FILE:launch_diagnostics_probe>" ${CMAKE_CROSSCOMPILING_EMULATOR})
set_tests_properties(launch_diagnostics_acceptance PROPERTIES TIMEOUT 45)
