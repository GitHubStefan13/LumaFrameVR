set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/manifest.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/manifest.json" LUMAFRAMEVR_SHADER_MANIFEST)
foreach(group runtime reference)
  string(JSON count LENGTH "${LUMAFRAMEVR_SHADER_MANIFEST}" ${group})
  math(EXPR last "${count} - 1")
  set(LUMAFRAMEVR_${group}_SHADERS)
  foreach(index RANGE ${last})
    string(JSON shader GET "${LUMAFRAMEVR_SHADER_MANIFEST}" ${group} ${index})
    list(APPEND LUMAFRAMEVR_${group}_SHADERS "${shader}")
  endforeach()
endforeach()
set(LUMAFRAMEVR_ALL_SHADERS ${LUMAFRAMEVR_runtime_SHADERS} ${LUMAFRAMEVR_reference_SHADERS})
list(LENGTH LUMAFRAMEVR_ALL_SHADERS shader_count)
list(REMOVE_DUPLICATES LUMAFRAMEVR_ALL_SHADERS)
list(LENGTH LUMAFRAMEVR_ALL_SHADERS unique_shader_count)
if(NOT shader_count EQUAL unique_shader_count)
  message(FATAL_ERROR "Duplicate runtime/reference shader classification")
endif()
set(LUMAFRAMEVR_RUNTIME_SHADER_VALUES "")
foreach(shader IN LISTS LUMAFRAMEVR_runtime_SHADERS)
  string(APPEND LUMAFRAMEVR_RUNTIME_SHADER_VALUES "    \"${shader}.spv\",\n")
endforeach()
list(LENGTH LUMAFRAMEVR_runtime_SHADERS runtime_shader_count)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/runtime_shaders.hpp" CONTENT
"#pragma once
#include <array>
#include <string_view>
namespace lumaframevr::application {
inline constexpr std::array<std::string_view, ${runtime_shader_count}> runtime_shaders{
${LUMAFRAMEVR_RUNTIME_SHADER_VALUES}};
}
")
