include(cmake/ShaderManifest.cmake)
find_package(Vulkan REQUIRED)
# glslc comes from the glslc package (shaderc) or a Vulkan SDK on PATH. An
# explicit -DGLSLC_EXECUTABLE=... overrides the search.
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
set(LUMAFRAMEVR_SHADER_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${LUMAFRAMEVR_SHADER_DIRECTORY}")
set(LUMAFRAMEVR_SHADER_OUTPUTS)
foreach(shader IN LISTS LUMAFRAMEVR_ALL_SHADERS)
  set(output "${LUMAFRAMEVR_SHADER_DIRECTORY}/${shader}.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${GLSLC_EXECUTABLE}" -O -std=450 --target-env=vulkan1.3 "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/video_color.glsl" VERBATIM)
  list(APPEND LUMAFRAMEVR_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(lumaframevr_shaders ALL DEPENDS ${LUMAFRAMEVR_SHADER_OUTPUTS})
add_library(lumaframevr_vulkan cpp/graphics/vulkan_upload_transition.cpp cpp/graphics/vulkan_shader_module.cpp cpp/graphics/vulkan_device.cpp cpp/graphics/vulkan_host_buffer.cpp cpp/graphics/vulkan_submission.cpp cpp/graphics/vulkan_compute_kernel.cpp cpp/graphics/vulkan_sampled_image.cpp cpp/graphics/vulkan_sampling.cpp cpp/graphics/vulkan_image_upload.cpp cpp/graphics/vulkan_video_pipeline.cpp cpp/graphics/vulkan_render_target.cpp cpp/graphics/vulkan_depth_attachment.cpp cpp/graphics/vulkan_video_image.cpp cpp/graphics/vulkan_video_conversion.cpp cpp/graphics/vulkan_dma_import.cpp cpp/graphics/vulkan_foreign_transition.cpp cpp/graphics/vulkan_yuv_upload.cpp)
target_include_directories(lumaframevr_vulkan PUBLIC cpp/graphics)
target_link_libraries(lumaframevr_vulkan PUBLIC Vulkan::Vulkan lumaframevr_presentation lumaframevr_media)
target_compile_options(lumaframevr_vulkan PRIVATE -Wall -Wextra -Wpedantic -Werror)
