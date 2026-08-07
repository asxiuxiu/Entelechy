#pragma once

namespace Entelechy
{

class IWindow
{
public:
    virtual ~IWindow() = default;

    virtual bool create(int width, int height, const char *title) = 0;
    virtual void destroy() = 0;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;
    virtual void getSize(int &width, int &height) const = 0;
    virtual void setSize(int width, int height) = 0;
    virtual void *getNativeHandle() const = 0;
    virtual void *getNativeDisplay() const = 0;
    virtual void swapBuffers() = 0;
    virtual void makeContextCurrent() = 0;

    // Poll the current pressed state of a key (GLFW key code). Unlike the
    // event queue this reflects the OS state at the last pollEvents() call,
    // so it is immune to lost press/release events (focus switches, IME
    // interception, global hotkeys). Prefer this for held-key queries
    // (movement); use events for edge-triggered actions.
    virtual bool isKeyDown(int keyCode) const = 0;

    // Whether the window currently has input focus.
    virtual bool hasFocus() const = 0;
};

} // namespace Entelechy
