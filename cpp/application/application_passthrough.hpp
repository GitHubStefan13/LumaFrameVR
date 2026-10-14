#pragma once
#include "application_controller.hpp"
#include "openxr_passthrough.hpp"
namespace lumaframevr::application {
void synchronize_passthrough(Controller& app, OpenXrPassthrough& camera, bool renderable, bool fault);
}
