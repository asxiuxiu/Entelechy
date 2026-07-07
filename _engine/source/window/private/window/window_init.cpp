#include "window/window_init.h"
#include "log/core/log_macros.h"
#include <GLFW/glfw3.h>
#include <cstdio>

namespace Entelechy
{

void initWindow()
{
    if (!glfwInit())
    {
        LOG_ERROR(LogCategories::kWindow, "glfwInit() failed!");
        return;
    }
    LOG_INFO(LogCategories::kWindow, "Window initialized (GLFW %s)", glfwGetVersionString());
}

} // namespace Entelechy
