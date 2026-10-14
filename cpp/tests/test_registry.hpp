#pragma once
// Test entry points for the single lumaframevr_tests binary.
//
// Each function runs one acceptance test and returns 0 on success. Tests that
// need a fixture path or a mode receive it through argv, exactly as they did
// when each was its own executable. Keeping them in one binary removes a dozen
// link steps and a dozen build targets without changing what is asserted.
namespace lumaframevr::tests {
int application_options(int argc, char** argv);
int browser(int argc, char** argv);
int browser_viewport(int argc, char** argv);
int catalogue(int argc, char** argv);
int controller(int argc, char** argv);
int controller_bindings(int argc, char** argv);
int dma_import(int argc, char** argv);
int gpu_color(int argc, char** argv);
int image_loader(int argc, char** argv);
int image_reader(int argc, char** argv);
int localization(int argc, char** argv);
int media(int argc, char** argv);
int media_errors(int argc, char** argv);
int audio_native(int argc, char** argv);
int audio_operations(int argc, char** argv);
int packet_reader(int argc, char** argv);
int packet_limits(int argc, char** argv);
int seek_preroll(int argc, char** argv);
int presentation_native(int argc, char** argv);
int v4l2_formats(int argc, char** argv);
int v4l2_layout(int argc, char** argv);
int v4l2_queue(int argc, char** argv);
int v4l2_device(int argc, char** argv);
int v4l2_bitstream(int argc, char** argv);
int media_loop(int argc, char** argv);
int presentation(int argc, char** argv);
int routing(int argc, char** argv);
int shader_module(int argc, char** argv);
int source(int argc, char** argv);
int timeline(int argc, char** argv);
int ui_contract(int argc, char** argv);
}  // namespace lumaframevr::tests
