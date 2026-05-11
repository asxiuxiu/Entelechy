#pragma once
#include "render_backend.h"
#include "foundation_types.h"

namespace Entelechy {

class IWindow;

class OpenGLBackend : public IRenderBackend {
public:
    explicit OpenGLBackend(IWindow* window);
    ~OpenGLBackend() override;

    bool init(void* nativeWindow) override;
    void shutdown() override;
    void beginFrame() override;
    void endFrame() override;
    void setViewport(int x, int y, int width, int height) override;
    void setClearColor(f32 r, f32 g, f32 b, f32 a) override;
    void clear() override;
    void present() override;

private:
    IWindow* m_window;
    RenderSettings m_settings;
};

} // namespace Entelechy
