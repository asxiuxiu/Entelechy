#pragma once
#include "render_world/ExtractSchedule.h"

namespace Entelechy {

class IWindow;

// ExtractCameraSystem — copies the first main-world camera into an ExtractedView
// in the render world. Uses the window size to compute aspect ratio and viewport.
class ExtractCameraSystem : public IExtractSystem {
public:
    explicit ExtractCameraSystem(IWindow* window) : m_window(window) {}

    void extract(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) override;

private:
    IWindow* m_window = nullptr;
};

} // namespace Entelechy
