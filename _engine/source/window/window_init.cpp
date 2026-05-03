#include "window_init.h"
#include <GLFW/glfw3.h>
#include <cstdio>

namespace Entelechy {

void initWindow() {
    if (!glfwInit()) {
        printf("[Entelechy::window] glfwInit() failed!\n");
        return;
    }
    printf("[Entelechy::window] initialized (GLFW %s)\n", glfwGetVersionString());
}

} // namespace Entelechy
