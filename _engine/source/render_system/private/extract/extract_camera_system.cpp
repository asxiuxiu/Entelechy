#include "render_system/extract/extract_camera_system.h"
#include "window/window.h"
#include "render_system/components/camera.h"
#include "ecs/component/transform_component.h"
#include "render_system/components/render_camera.h"
#include "render_system/culling/view_visible_list.h"
#include "render_system/phase/render_resources.h"
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
        // Camera world position = translation column of the camera transform.
        const Vec3 view_pos{transform->matrix.m[12], transform->matrix.m[13], transform->matrix.m[14]};

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
            ev->view_pos = view_pos;
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
            view.view_pos = view_pos;
            renderWorld.addComponent(viewEntity, view);

            // Pre-bind downstream view resources to the same entity so that
            // FrustumCullSystem and QueueDrawsSystem can find them with O(1)
            // lookups instead of spawning separate entities each frame.
            renderWorld.addComponent(viewEntity, ViewVisibleList{});
            renderWorld.addComponent(viewEntity, ViewBinnedPhases{});
            renderWorld.addComponent(viewEntity, ViewSortedPhases{});
        }

        // Only the first camera is extracted.
        break;
    }
}

} // namespace Entelechy
