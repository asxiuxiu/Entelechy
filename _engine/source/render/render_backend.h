#pragma once

namespace Entelechy {

struct RenderSettings {
    // Eye-care dark green-grey; softer than pure black/white to reduce long-term eye strain
    float clearColor[4] = {0.15f, 0.17f, 0.13f, 1.0f};
    bool vsync = true;
};

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual bool init(void* nativeWindow) = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void clear() = 0;
    virtual void present() = 0;
};

} // namespace Entelechy
