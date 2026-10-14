include(cmake/ControllerModels.cmake)

add_library(lumaframevr_application_runtime
  cpp/application/application_assets.cpp
  cpp/application/launch_diagnostics.cpp
  cpp/application/vulkan_overlay.cpp
  cpp/application/controller_mesh.cpp
  cpp/application/controller_mesh_pipeline.cpp
  cpp/application/controller_models_renderer.cpp
  cpp/application/controller_layer.cpp
  cpp/application/controller_composition.cpp
  cpp/application/beam_surface.cpp
  cpp/application/video_surface.cpp
  cpp/application/application_renderer.cpp
  cpp/application/gpu_preview.cpp
  cpp/application/application_panels.cpp
  cpp/application/application_run.cpp
  cpp/application/application_passthrough.cpp
  cpp/application/application_probe.cpp)
target_link_libraries(lumaframevr_application_runtime PUBLIC lumaframevr_application_controller lumaframevr_probe_core controller_model_runtime)
target_compile_options(lumaframevr_application_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(lumaframevr_application_runtime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(BUILD_TESTING)
add_executable(launch_diagnostics_probe cpp/tests/launch_diagnostics_probe.cpp cpp/application/launch_diagnostics.cpp)
target_include_directories(launch_diagnostics_probe PRIVATE cpp/application)
target_compile_options(launch_diagnostics_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(lumaframevr cpp/application/main.cpp)
target_link_libraries(lumaframevr PRIVATE lumaframevr_application_runtime)
target_compile_options(lumaframevr PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(lumaframevr PRIVATE LUMAFRAMEVR_VERSION="${PROJECT_VERSION}")
add_dependencies(lumaframevr lumaframevr_shaders)
get_target_property(LUMAFRAMEVR_COMPONENT_BINARY lumaframevr_shaders BINARY_DIR)
foreach(shader IN LISTS LUMAFRAMEVR_runtime_SHADERS)
  add_custom_command(TARGET lumaframevr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lumaframevr>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${LUMAFRAMEVR_COMPONENT_BINARY}/shaders/${shader}.spv"
      "$<TARGET_FILE_DIR:lumaframevr>/shaders/${shader}.spv" VERBATIM)
  install(FILES "${LUMAFRAMEVR_COMPONENT_BINARY}/shaders/${shader}.spv" DESTINATION share/lumaframevr/shaders)
endforeach()
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/" DESTINATION share/lumaframevr/branding)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/eye/" DESTINATION share/lumaframevr/eye)
install(TARGETS lumaframevr RUNTIME DESTINATION bin)
