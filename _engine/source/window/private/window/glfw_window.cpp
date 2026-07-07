#include "window/glfw_window.h"
#include "window/input/input_queue.h"
#include "window/input/input_event.h"
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Entelechy
{

static void glfwKeyCallback(GLFWwindow * /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    RawInputEvent event;
    if (action == GLFW_PRESS)
    {
        event.type = RawInputEvent::KeyPress;
    }
    else if (action == GLFW_RELEASE)
    {
        event.type = RawInputEvent::KeyRelease;
    }
    else
    {
        return;
    }
    event.keyCode = key;
    InputQueue::instance().push(event);
}

static void glfwMouseButtonCallback(GLFWwindow * /*window*/, int button, int action, int /*mods*/)
{
    RawInputEvent event;
    event.type = (action == GLFW_PRESS) ? RawInputEvent::MouseButtonPress : RawInputEvent::MouseButtonRelease;
    event.mouseButton = button;
    InputQueue::instance().push(event);
}

static void glfwCursorPosCallback(GLFWwindow *window, double x, double y)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Filter out cursor movement outside the window client area
    // (GLFW still fires this callback during drag-outside-window)
    if (x < 0 || y < 0 || x >= width || y >= height)
    {
        return;
    }

    RawInputEvent event;
    event.type = RawInputEvent::MouseMove;
    event.mx = static_cast<f32>(x);
    event.my = static_cast<f32>(y);
    InputQueue::instance().push(event);
}

static void glfwWindowSizeCallback(GLFWwindow * /*window*/, int width, int height)
{
    RawInputEvent event;
    event.type = RawInputEvent::WindowResize;
    event.width = width;
    event.height = height;
    InputQueue::instance().push(event);
}

static void glfwWindowCloseCallback(GLFWwindow * /*window*/)
{
    RawInputEvent event;
    event.type = RawInputEvent::Close;
    InputQueue::instance().push(event);
}

GlfwWindow::GlfwWindow() : m_window(nullptr) {}

GlfwWindow::~GlfwWindow()
{
    if (m_window)
    {
        destroy();
    }
}

bool GlfwWindow::create(int width, int height, const char *title)
{
#if defined(__APPLE__)
    // macOS caps at OpenGL 4.1; Core Profile also requires FORWARD_COMPAT.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window)
    {
        return false;
    }

    glfwMakeContextCurrent(m_window);

    glfwSetKeyCallback(m_window, glfwKeyCallback);
    glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
    glfwSetWindowSizeCallback(m_window, glfwWindowSizeCallback);
    glfwSetWindowCloseCallback(m_window, glfwWindowCloseCallback);

    return true;
}

void GlfwWindow::centerOnScreen()
{
    if (!m_window)
    {
        return;
    }

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        return;
    }

    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
    {
        return;
    }

    int windowWidth, windowHeight;
    glfwGetWindowSize(m_window, &windowWidth, &windowHeight);

    int x = (mode->width - windowWidth) / 2;
    int y = (mode->height - windowHeight) / 2;
    glfwSetWindowPos(m_window, x, y);
}

void GlfwWindow::getPrimaryMonitorSize(int &width, int &height)
{
    width = 1920;
    height = 1080;

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        return;
    }

    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (mode)
    {
        width = mode->width;
        height = mode->height;
    }
}

void GlfwWindow::getPrimaryMonitorContentScale(f32 &xscale, f32 &yscale)
{
    xscale = 1.0f;
    yscale = 1.0f;

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        return;
    }

    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
}

void GlfwWindow::getRecommendedWindowSize(int &width, int &height, f32 fraction)
{
    int monitorW, monitorH;
    getPrimaryMonitorSize(monitorW, monitorH);

    width = static_cast<int>(monitorW * fraction);
    height = static_cast<int>(monitorH * fraction);

    // Clamp to reasonable bounds.
    const int minW = 1280, minH = 720;
    const int maxW = 1920, maxH = 1080;

    if (width < minW)
        width = minW;
    if (height < minH)
        height = minH;
    if (width > maxW)
        width = maxW;
    if (height > maxH)
        height = maxH;
}

void GlfwWindow::destroy()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void GlfwWindow::pollEvents()
{
    glfwPollEvents();
}

bool GlfwWindow::shouldClose() const
{
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void GlfwWindow::requestClose()
{
    if (m_window)
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void GlfwWindow::getSize(int &width, int &height) const
{
    if (m_window)
    {
        glfwGetWindowSize(m_window, &width, &height);
    }
    else
    {
        width = 0;
        height = 0;
    }
}

void GlfwWindow::setSize(int width, int height)
{
    if (m_window)
    {
        glfwSetWindowSize(m_window, width, height);
    }
}

void *GlfwWindow::getNativeHandle() const
{
    if (!m_window)
    {
        return nullptr;
    }
#if defined(_WIN32)
    return glfwGetWin32Window(m_window);
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(m_window);
#elif defined(__linux__)
    return reinterpret_cast<void *>(glfwGetX11Window(m_window));
#else
    return nullptr;
#endif
}

void *GlfwWindow::getNativeDisplay() const
{
    if (!m_window)
    {
        return nullptr;
    }
    // Phase 1 stub: reserved for Vulkan surface creation.
    // Full implementation will use platform-specific APIs:
    //   Windows: GetModuleHandle(nullptr) as HINSTANCE
    //   Linux:   glfwGetX11Display() as Display*
    //   macOS:   not applicable (CAMetalLayer extracted from NSView)
    return nullptr;
}

void GlfwWindow::swapBuffers()
{
    if (m_window)
    {
        glfwSwapBuffers(m_window);
    }
}

void GlfwWindow::makeContextCurrent()
{
    if (m_window)
    {
        glfwMakeContextCurrent(m_window);
    }
}

} // namespace Entelechy
