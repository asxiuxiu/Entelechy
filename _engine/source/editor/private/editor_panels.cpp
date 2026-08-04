#include "editor/editor_panels.h"
#include <imgui.h>
#include "ecs/world/world.h"
#include "ecs/world/scheduler.h"
#include "ecs/type/type_registry.h"
#include "core/math/vec.h"
#include "core/math/quat.h"
#include "core/math/mat4.h"
#include "ecs/component/transform_component.h"
#include "ecs/type/atom_registry.h"
#include "core/string/string_intern_pool.h"
#include <ctime>
#include <chrono>
#include <cstdio>

namespace Entelechy
{

namespace
{

bool drawField(const Entelechy::FieldDesc &field, void *componentRaw)
{
    void *fieldPtr = static_cast<u8 *>(componentRaw) + field.offset;
    bool changed = false;

    // 1. Try AtomRegistry for true atoms (f32, bool, i32, u32, StringId, String)
    const char *fieldNameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
    if (Entelechy::AtomRegistry::instance().tryDraw(field.type, fieldNameResolved ? fieldNameResolved : "", fieldPtr))
    {
        changed = true;
        return changed;
    }

    // 2. Special-case Mat4 for better UX (4 rows instead of 16 raw floats)
    if (field.type == "Mat4"_sid)
    {
        const char *fieldNameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(fieldNameResolved ? fieldNameResolved : ""))
        {
            auto *mat = static_cast<Entelechy::Mat4 *>(fieldPtr);
            for (int row = 0; row < 4; ++row)
            {
                float rowVals[4];
                for (int col = 0; col < 4; ++col)
                    rowVals[col] = (*mat)(row, col);
                char rowLabel[8];
                snprintf(rowLabel, sizeof(rowLabel), "[%d]", row);
                if (ImGui::DragFloat4(rowLabel, rowVals, 0.01f))
                    changed = true;
                for (int col = 0; col < 4; ++col)
                    (*mat)(row, col) = rowVals[col];
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 3. Try TypeRegistry composite lookup (Vec3, Quat, Vec2, Vec4, Entity, Color, ...)
    const auto *typeDesc = Entelechy::TypeRegistry::instance().findType(field.type);
    if (typeDesc && typeDesc->kind == Entelechy::TypeKind::Composite && !typeDesc->fields.empty())
    {
        const char *nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(nameResolved ? nameResolved : ""))
        {
            for (const auto &subField : typeDesc->fields)
            {
                if (drawField(subField, fieldPtr))
                    changed = true;
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 4. Fallback: legacy ComponentDesc recursive lookup
    const auto *compDesc = Entelechy::TypeRegistry::instance().findComponent(field.type);
    if (compDesc && !compDesc->fields.empty())
    {
        const char *nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(nameResolved ? nameResolved : ""))
        {
            for (const auto &subField : compDesc->fields)
            {
                if (drawField(subField, fieldPtr))
                    changed = true;
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 5. Unknown type
    const char *nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
    const char *typeResolved = Entelechy::StringInternPool::instance().resolve(field.type);
    ImGui::TextDisabled("%s: (%s)", nameResolved ? nameResolved : "", typeResolved ? typeResolved : "");
    return changed;
}

} // anonymous namespace

void buildECSInspector(World &world, Scheduler &scheduler, f32 dt, bool &autoRun)
{
    static Entity selected{0xFFFFFFFF, 0};

    // ---------- Left panel: ECS World ----------
    ImGui::SetNextWindowPos(ImVec2(20, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("ECS World");

    if (ImGui::Button("Add Entity"))
    {
        Entity e = world.spawn();
        world.addComponent<Position>(e, {0.0f, 0.0f});
        world.addComponent<Velocity>(e, {1.0f, 0.0f});
        world.addComponent<Health>(e, {100.0f});
        world.addComponent<NameTag>(e, {StringInternPool::instance().intern("NewEntity")});
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected") && selected.valid() && world.valid(selected))
    {
        world.destroy(selected);
        selected = {0xFFFFFFFF, 0};
    }

    ImGui::Separator();
    ImGui::Checkbox("Auto Run Systems", &autoRun);
    ImGui::SameLine();
    if (ImGui::Button("Tick Once"))
    {
        scheduler.tick(world, dt);
    }
    ImGui::Separator();

    usize aliveCount = world.entityCount();
    ImGui::Text("Entities (%zu alive):", aliveCount);

    for (u32 id = 0; id < world.maxEntityID(); ++id)
    {
        Entity e{id, world.getEntityGeneration(id)};
        if (!world.valid(e))
            continue;

        String label = formatString("Entity {0}", static_cast<int>(e.id));
        bool hasAny = false;
        for (const auto &pair : world.componentArrays())
        {
            if (pair.second->has(e))
            {
                const auto *desc = TypeRegistry::instance().findComponent(pair.first);
                if (desc)
                {
                    if (hasAny)
                    {
                        label += ", ";
                    }
                    else
                    {
                        label += " [";
                    }
                    const char *nameResolved = Entelechy::StringInternPool::instance().resolve(desc->name);
                    if (nameResolved)
                        label += nameResolved;
                    hasAny = true;
                }
            }
        }
        if (hasAny)
            label += "]";

        bool isSelected = (selected == e);
        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            selected = e;
        }
    }
    ImGui::End();

    // ---------- Right panel: Inspector ----------
    ImGui::SetNextWindowPos(ImVec2(360, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (selected.valid() && world.valid(selected))
    {
        ImGui::Text("Entity ID: %u, Generation: %u", selected.id, selected.generation);
        ImGui::Separator();

        for (const auto &pair : world.componentArrays())
        {
            if (!pair.second->has(selected))
                continue;
            const auto *desc = TypeRegistry::instance().findComponent(pair.first);
            if (!desc)
                continue;
            void *raw = const_cast<void *>(pair.second->getRaw(selected));
            if (!raw)
                continue;

            const char *nameResolved = Entelechy::StringInternPool::instance().resolve(desc->name);
            if (ImGui::TreeNodeEx(nameResolved ? nameResolved : "", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool componentChanged = false;
                for (const auto &field : desc->fields)
                {
                    if (drawField(field, raw))
                        componentChanged = true;
                }
                ImGui::TreePop();
                // If the Transform component was edited, mark it dirty so the
                // TransformPropagationSystem recomputes the world matrix.
                if (componentChanged && desc->name == "Transform"_sid)
                {
                    if (auto *trans = world.getComponent<Entelechy::Transform>(selected))
                    {
                        trans->dirty = 1;
                    }
                }
            }
        }
    }
    else
    {
        ImGui::TextDisabled("No entity selected.");
    }
    ImGui::End();
}

} // namespace Entelechy
