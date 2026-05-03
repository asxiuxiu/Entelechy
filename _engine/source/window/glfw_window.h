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

private:
    GLFWwindow* m_window;
};

} // namespace Entelechy
