#pragma once
#include "window.h"

struct GLFWwindow;

namespace Entelechy {

class GlfwWindow : public IWindow {
public:
    GlfwWindow();
    ~GlfwWindow() override;

    bool create(int width, int height, const char* title) override;
    void destroy() override;
    void pollEvents() override;
    bool shouldClose() const override;
    void requestClose() override;
    void getSize(int& width, int& height) const override;
    void setSize(int width, int height) override;
    void* getNativeHandle() const override;
    void* getNativeDisplay() const override;
    GLFWwindow* getGlfwWindow() const { return m_window; }
    void swapBuffers() override;
    void makeContextCurrent() override;

    // Center the window on the primary monitor.
    void centerOnScreen();

    // Static helpers for monitor / DPI queries.
    static void getPrimaryMonitorSize(int& width, int& height);
    static void getPrimaryMonitorContentScale(float& xscale, float& yscale);

    // Compute a sensible default window size based on the primary monitor.
    // Returns size as a fraction of the monitor (default 75%), clamped to
    // a minimum of 1280x720 and a maximum of 1920x1080.
    static void getRecommendedWindowSize(int& width, int& height, float fraction = 0.75f);

private:
    GLFWwindow* m_window;
};

} // namespace Entelechy
