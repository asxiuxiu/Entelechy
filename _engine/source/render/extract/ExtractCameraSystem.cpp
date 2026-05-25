#include "extract/ExtractCameraSystem.h"
#include "window.h"
#include "components/Camera.h"
#include "math/transform_component.h"
#include "components/RenderCamera.h"
#include "math/mat4.h"
#include "query.h"

namespace Entelechy {

void ExtractCameraSystem::extract(const World& mainWorld, World& renderWorld, FrameArena& arena, f32 dt) {
    if (!m_window) return;

    int w = 0, h = 0;
    m_window->getSize(w, h);
    if (w <= 0 || h <= 0) return;

    f32 aspect = static_cast<f32>(w) / static_cast<f32>(h);

    ConstQuery<Camera, GlobalTransform> q(mainWorld);
    for (auto [entity, camera, transform] : q) {
        Mat4 view_matrix = transform->matrix.inverse();
        Mat4 proj_matrix;

        if (camera->orthographic) {
            f32 half_h = camera->ortho_size;
            f32 half_w = half_h * aspect;
            proj_matrix = Mat4::ortho(-half_w, half_w, -half_h, half_h,
                                       camera->near_plane, camera->far_plane);
        } else {
            proj_matrix = Mat4::perspective(camera->fov_y, aspect,
                                             camera->near_plane, camera->far_plane);
        }

        // Update existing ExtractedView or create a new one.
        Query<ExtractedView> rq(renderWorld);
        bool found = false;
        for (auto [re, ev] : rq) {
            ev->view_matrix = view_matrix;
            ev->proj_matrix = proj_matrix;
            ev->frustum = Frustum::fromMatrix(proj_matrix * view_matrix);
            ev->viewport = Rect{0.0f, 0.0f, static_cast<f32>(w), static_cast<f32>(h)};
            found = true;
            break;
        }

        if (!found) {
            Entity viewEntity = renderWorld.spawn();
            ExtractedView view{};
            view.view_matrix = view_matrix;
            view.proj_matrix = proj_matrix;
            view.frustum = Frustum::fromMatrix(proj_matrix * view_matrix);
            view.viewport = Rect{0.0f, 0.0f, static_cast<f32>(w), static_cast<f32>(h)};
            renderWorld.addComponent(viewEntity, view);
        }

        // Checkpoint 1: only the first camera is extracted.
        break;
    }
}

} // namespace Entelechy
