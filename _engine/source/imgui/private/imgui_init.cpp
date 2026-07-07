#include "imgui/imgui_init.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

void initImGui()
{
    // Actual ImGui context + backend initialization happens in ImGuiManager::init(),
    // which requires a live GLFW window and OpenGL context. That call is made
    // from the generated main.cpp after window creation.
    // This stub exists so the Source Forest generator can emit a uniform init call.
    LOG_INFO(LogCategories::kEngine, "ImGui module registered (late-init via ImGuiManager)");
}

} // namespace Entelechy
