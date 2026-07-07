#include "render/rhi/opengl_backend.h"
#include "window/window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "log/core/log_macros.h"

namespace Entelechy
{

OpenGLBackend::OpenGLBackend(IWindow *window) : m_window(window) {}

OpenGLBackend::~OpenGLBackend()
{
    if (m_window)
    {
        shutdown();
    }
}

bool OpenGLBackend::init(void * /*nativeWindow*/)
{
    if (!m_window)
    {
        return false;
    }
    m_window->makeContextCurrent();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LOG_ERROR(Entelechy::LogCategories::kEngine, "GLAD init failed");
        return false;
    }

    LOG_INFO(Entelechy::LogCategories::kEngine, "OpenGL %s | GPU: %s", glGetString(GL_VERSION),
             glGetString(GL_RENDERER));

    glfwSwapInterval(m_settings.vsync ? 1 : 0);

    int w, h;
    m_window->getSize(w, h);
    glViewport(0, 0, w, h);
    return true;
}

void OpenGLBackend::shutdown()
{
    m_window = nullptr;
}

void OpenGLBackend::beginFrame()
{
    int width, height;
    m_window->getSize(width, height);
    glViewport(0, 0, width, height);
}

void OpenGLBackend::endFrame() {}

void OpenGLBackend::setViewport(int x, int y, int width, int height)
{
    glViewport(x, y, width, height);
}

void OpenGLBackend::setClearColor(f32 r, f32 g, f32 b, f32 a)
{
    m_settings.clearColor[0] = r;
    m_settings.clearColor[1] = g;
    m_settings.clearColor[2] = b;
    m_settings.clearColor[3] = a;
}

void OpenGLBackend::clear()
{
    glClearColor(m_settings.clearColor[0], m_settings.clearColor[1], m_settings.clearColor[2],
                 m_settings.clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::present()
{
    m_window->swapBuffers();
}

} // namespace Entelechy
