find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(MEDIA REQUIRED IMPORTED_TARGET libavformat>=61 libavcodec>=61 libavutil>=59 libswresample>=5 libpulse)
add_library(lumaframevr_native STATIC
  cpp/media/media.cpp cpp/media/media_errors.cpp cpp/media/media_frame.cpp cpp/media/media_seek.cpp
  cpp/media/media_codec.cpp cpp/media/media_decoder_startup.cpp cpp/media/media_open.cpp cpp/media/media_description.cpp
  cpp/media/media_frame_color.cpp cpp/media/media_frame_layout.cpp cpp/media/media_receive.cpp
  cpp/media/media_trace.cpp cpp/media/media_packets.cpp cpp/media/media_reader.cpp cpp/media/media_metadata.cpp
  cpp/media/v4l2_format.cpp cpp/media/v4l2_decoder.cpp cpp/media/media_v4l2.cpp
  cpp/media/audio.cpp cpp/media/audio_decode.cpp cpp/media/audio_output.cpp cpp/media/audio_control.cpp cpp/media/audio_volume.cpp)
target_include_directories(lumaframevr_native PRIVATE cpp/media)
target_compile_options(lumaframevr_native PRIVATE -Wall -Wextra -Werror -fPIC)
target_link_libraries(lumaframevr_native PUBLIC PkgConfig::MEDIA Threads::Threads m)
add_library(lumaframevr_media cpp/media/decoder_session.cpp cpp/media/decoder_operations.cpp cpp/media/retained_frame.cpp cpp/media/software_pixel_payload.cpp)
target_include_directories(lumaframevr_media PUBLIC cpp/media)
target_link_libraries(lumaframevr_media PUBLIC lumaframevr_source PRIVATE lumaframevr_native)
target_compile_options(lumaframevr_media PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_library(lumaframevr_playback cpp/playback/decode_worker.cpp cpp/playback/decode_worker_loop.cpp cpp/playback/playback_timeline.cpp cpp/playback/playback_presentation.cpp)
target_include_directories(lumaframevr_playback PUBLIC cpp/playback)
target_link_libraries(lumaframevr_playback PUBLIC lumaframevr_media PRIVATE Threads::Threads)
target_compile_options(lumaframevr_playback PRIVATE -Wall -Wextra -Wpedantic -Werror)
