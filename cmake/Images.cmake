add_library(lumaframevr_images STATIC
  cpp/media/image_reader.cpp cpp/media/image_conversion.cpp
  cpp/media/image_orientation.cpp cpp/media/image_loader.cpp
  cpp/media/image_formats.cpp cpp/media/image_thumbnail.cpp)
target_include_directories(lumaframevr_images PUBLIC cpp/media)
target_link_libraries(lumaframevr_images PRIVATE PkgConfig::MEDIA Threads::Threads lumaframevr_source)
target_compile_options(lumaframevr_images PRIVATE -Wall -Wextra -Wpedantic -Werror)
set(LUMAFRAMEVR_IMAGE_FIXTURES "$ENV{LUMAFRAMEVR_IMAGE_FIXTURES}" CACHE PATH "Still-image regression fixture directory")
function(lumaframevr_image_runtime_tests)
  if(TARGET lumaframevr_thumbnails)
    target_link_libraries(lumaframevr_thumbnails PRIVATE lumaframevr_images)
  endif()
endfunction()
cmake_language(DEFER CALL lumaframevr_image_runtime_tests)
