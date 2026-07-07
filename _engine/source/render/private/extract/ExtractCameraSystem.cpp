#include "render/extract/ExtractCameraSystem.h"
#include "window/window.h"
#include "render/components/Camera.h"
#include "ecs/component/transform_component.h"
#include "render/components/RenderCamera.h"
#include "render/culling/ViewVisibleList.h"
#include "render/phase/RenderResources.h"
#include "core/math/mat4.h"
#include "ecs/query/query.h"

namespace Entelechy
{

void ExtractCameraSystem::extract(const World &mainWorld, World &renderWorld, FrameArena &arena, f32 dt)
{
    if (!m_window)
        return;

    int w = 0, h = 0;
    m_window->getSize(w, h);
    if (w <= 0 || h <= 0)
        return;

    f32 aspect = static_cast<f32>(w) / static_cast<f32>(h);

    ConstQuery<Camera, GlobalTransform> q(mainWorld);
    for (auto [entity, camera, transform] : q)
    {
        Mat4 view_matrix = transform->matrix.inverse();
        Mat4 proj_matrix;

        if (camera->orthographic)
        {
            f32 half_h = camera->ortho_size;
            f32 half_w = half_h * aspect;
            proj_matrix = Mat4::ortho(-half_w, half_w, -half_h, half_h, camera->near_plane, camera->far_plane);
        }
        else
        {
            proj_matrix = Mat4::perspective(camera->fov_y, aspect, camera->near_plane, camera->far_plane);
        }

        // Update existing ExtractedView or create a new one.
        Query<ExtractedView> rq(renderWorld);
        bool found = false;
        for (auto [re, ev] : rq)
        {
            ev->view_matrix = view_matrix;
            ev->proj_matrix = proj_matrix;
            ev->frustum = Frustum::fromMatrix(proj_matrix * view_matrix);
            ev->viewport = Rect{0.0f, 0.0f, static_cast<f32>(w), static_cast<f32>(h)};
            ev->near_plane = camera->near_plane;
            ev->far_plane = camera->far_plane;
            found = true;
            break;
        }

        if (!found)
        {
            Entity viewEntity = renderWorld.spawn();
            ExtractedView view{};
            view.view_matrix = view_matrix;
            view.proj_matrix = proj_matrix;
            view.frustum = Frustum::fromMatrix(proj_matrix * view_matrix);
            view.viewport = Rect{0.0f, 0.0f, static_cast<f32>(w), static_cast<f32>(h)};
            view.near_plane = camera->near_plane;
            view.far_plane = camera->far_plane;
            renderWorld.addComponent(viewEntity, view);

            // Pre-bind downstream view resources to the same entity so that
            // FrustumCullSystem and QueueDrawsSystem can find them with O(1)
            // lookups instead of spawning separate entities each frame.
            renderWorld.addComponent(viewEntity, ViewVisibleList{});
            renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
            renderWorld.addComponent(viewEntity, ViewSortedPhases{});
        }

        // Checkpoint 1: only the first camera is extracted.
        break;
    }
}

} // namespace Entelechy
