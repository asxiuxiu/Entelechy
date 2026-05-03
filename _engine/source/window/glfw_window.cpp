#include "glfw_window.h"
#include "input_queue.h"
#include "input_event.h"
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Entelechy {

static void glfwKeyCallback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/) {
    RawInputEvent event;
    if (action == GLFW_PRESS) {
        event.type = RawInputEvent::KeyPress;
    } else if (action == GLFW_RELEASE) {
        event.type = RawInputEvent::KeyRelease;
    } else {
        return;
    }
    event.keyCode = key;
    InputQueue::instance().push(event);
}

static void glfwMouseButtonCallback(GLFWwindow* /*window*/, int button, int action, int /*mods*/) {
    RawInputEvent event;
    event.type = (action == GLFW_PRESS) ? RawInputEvent::MouseButtonPress : RawInputEvent::MouseButtonRelease;
    event.mouseButton = button;
    InputQueue::instance().push(event);
}

static void glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Filter out cursor movement outside the window client area
    // (GLFW still fires this callback during drag-outside-window)
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    RawInputEvent event;
    event.type = RawInputEvent::MouseMove;
    event.mx = static_cast<float>(x);
    event.my = static_cast<float>(y);
    InputQueue::instance().push(event);
}

static void glfwWindowSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    RawInputEvent event;
    event.type = RawInputEvent::WindowResize;
    event.width = width;
    event.height = height;
    InputQueue::instance().push(event);
}

static void glfwWindowCloseCallback(GLFWwindow* /*window*/) {
    RawInputEvent event;
    event.type = RawInputEvent::Close;
    InputQueue::instance().push(event);
}

GlfwWindow::GlfwWindow()
    : m_window(nullptr) {
}

GlfwWindow::~GlfwWindow() {
    if (m_window) {
        destroy();
    }
}

bool GlfwWindow::create(int width, int height, const char* title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        return false;
    }

    glfwSetKeyCallback(m_window, glfwKeyCallback);
    glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
    glfwSetWindowSizeCallback(m_window, glfwWindowSizeCallback);
    glfwSetWindowCloseCallback(m_window, glfwWindowCloseCallback);

    return true;
}

void GlfwWindow::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void GlfwWindow::pollEvents() {
    glfwPollEvents();
}

bool GlfwWindow::shouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void GlfwWindow::requestClose() {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void GlfwWindow::getSize(int& width, int& height) const {
    if (m_window) {
        glfwGetWindowSize(m_window, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}

void GlfwWindow::setSize(int width, int height) {
    if (m_window) {
        glfwSetWindowSize(m_window, width, height);
    }
}

void* GlfwWindow::getNativeHandle() const {
    if (!m_window) {
        return nullptr;
    }
#if defined(_WIN32)
    return glfwGetWin32Window(m_window);
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(m_window);
#elif defined(__linux__)
    return reinterpret_cast<void*>(glfwGetX11Window(m_window));
#else
    return nullptr;
#endif
}

void* GlfwWindow::getNativeDisplay() const {
    if (!m_window) {
        return nullptr;
    }
    // Phase 1 stub: reserved for Vulkan surface creation.
    // Full implementation will use platform-specific APIs:
    //   Windows: GetModuleHandle(nullptr) as HINSTANCE
    //   Linux:   glfwGetX11Display() as Display*
    //   macOS:   not applicable (CAMetalLayer extracted from NSView)
    return nullptr;
}

} // namespace Entelechy
