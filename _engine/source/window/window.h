#pragma once

namespace Entelechy {

class IWindow {
public:
    virtual ~IWindow() = default;

    virtual bool create(int width, int height, const char* title) = 0;
    virtual void destroy() = 0;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;
    virtual void getSize(int& width, int& height) const = 0;
    virtual void setSize(int width, int height) = 0;
    virtual void* getNativeHandle() const = 0;
    virtual void* getNativeDisplay() const = 0;
};

} // namespace Entelechy
