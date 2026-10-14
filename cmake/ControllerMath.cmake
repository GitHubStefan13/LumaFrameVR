add_library(lumaframevr_controller_math cpp/input/pose_math.cpp cpp/input/controller_pose_filter.cpp cpp/input/panel_pointer.cpp cpp/input/seek_repeat.cpp)
target_include_directories(lumaframevr_controller_math PUBLIC cpp/input)
target_compile_options(lumaframevr_controller_math PRIVATE -Wall -Wextra -Wpedantic -Werror)
