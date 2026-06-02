#include "imgui/imgui_panels.h"
#include <imgui.h>
#include "log/core/logger.h"
#include "log/core/log_level.h"
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

namespace Entelechy {

namespace {

ImVec4 logLevelToColor(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return ImVec4(0.60f, 0.60f, 0.60f, 1.00f); // gray
        case LogLevel::Info:    return ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // near-white
        case LogLevel::Warning: return ImVec4(1.00f, 0.85f, 0.20f, 1.00f); // yellow
        case LogLevel::Error:   return ImVec4(1.00f, 0.25f, 0.25f, 1.00f); // red
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

const char* logLevelToShortString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "D";
        case LogLevel::Info:    return "I";
        case LogLevel::Warning: return "W";
        case LogLevel::Error:   return "E";
    }
    return "?";
}

bool drawField(const Entelechy::FieldDesc& field, void* componentRaw) {
    void* fieldPtr = static_cast<u8*>(componentRaw) + field.offset;
    bool changed = false;

    // 1. Try AtomRegistry for true atoms (f32, bool, i32, u32, StringId, String)
    const char* fieldNameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
    if (Entelechy::AtomRegistry::instance().tryDraw(field.type, fieldNameResolved ? fieldNameResolved : "", fieldPtr)) {
        changed = true;
        return changed;
    }

    // 2. Special-case Mat4 for better UX (4 rows instead of 16 raw floats)
    if (field.type == "Mat4"_sid) {
        const char* fieldNameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(fieldNameResolved ? fieldNameResolved : "")) {
            auto* mat = static_cast<Entelechy::Mat4*>(fieldPtr);
            for (int row = 0; row < 4; ++row) {
                float rowVals[4];
                for (int col = 0; col < 4; ++col) rowVals[col] = (*mat)(row, col);
                char rowLabel[8];
                snprintf(rowLabel, sizeof(rowLabel), "[%d]", row);
                if (ImGui::DragFloat4(rowLabel, rowVals, 0.01f)) changed = true;
                for (int col = 0; col < 4; ++col) (*mat)(row, col) = rowVals[col];
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 3. Try TypeRegistry composite lookup (Vec3, Quat, Vec2, Vec4, Entity, Color, ...)
    const auto* typeDesc = Entelechy::TypeRegistry::instance().findType(field.type);
    if (typeDesc && typeDesc->kind == Entelechy::TypeKind::Composite && !typeDesc->fields.empty()) {
        const char* nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(nameResolved ? nameResolved : "")) {
            for (const auto& subField : typeDesc->fields) {
                if (drawField(subField, fieldPtr)) changed = true;
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 4. Fallback: legacy ComponentDesc recursive lookup
    const auto* compDesc = Entelechy::TypeRegistry::instance().findComponent(field.type);
    if (compDesc && !compDesc->fields.empty()) {
        const char* nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
        if (ImGui::TreeNode(nameResolved ? nameResolved : "")) {
            for (const auto& subField : compDesc->fields) {
                if (drawField(subField, fieldPtr)) changed = true;
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // 5. Unknown type
    const char* nameResolved = Entelechy::StringInternPool::instance().resolve(field.name);
    const char* typeResolved = Entelechy::StringInternPool::instance().resolve(field.type);
    ImGui::TextDisabled("%s: (%s)", nameResolved ? nameResolved : "", typeResolved ? typeResolved : "");
    return changed;
}

} // anonymous namespace

void buildDockSpace() {
    // Create a full-screen invisible window that acts as the docking container.
    // PassthruCentralNode keeps the center area transparent so the 3D scene
    // shows through; docking panels snap around the edges.
    ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );
}

void buildDebugPanel(f32 dt, f32 fps, f32 clearColor[4],
                     int windowWidth, int windowHeight,
                     WindowSizeRequest& outRequest) {
    // First-use defaults: top-left corner, large enough for all controls.
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);

    ImGui::Begin("Debug");

    ImGui::Text("FPS: %.1f | Frame: %.3f ms", fps, dt * 1000.0f);
    ImGui::Separator();

    ImGui::ColorEdit3("Clear Color", clearColor);
    if (ImGui::Button("Reset Color")) {
        clearColor[0] = 0.15f;
        clearColor[1] = 0.17f;
        clearColor[2] = 0.13f;
    }

    ImGui::Separator();
    ImGui::Text("Mouse: %.0f, %.0f",
                ImGui::GetIO().MousePos.x,
                ImGui::GetIO().MousePos.y);

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", windowWidth, windowHeight);

    // Resolution presets — use a compact combo instead of a row of buttons
    // so the UI stays readable even on narrow panels.
    ImGui::Separator();
    ImGui::Text("Resolution:");

    static int selectedPreset = 2; // default to 1920x1080
    const char* comboItems = "HD (1280x720)\0HD+ (1600x900)\0FHD (1920x1080)\0Fit Monitor (75%)\0";
    if (ImGui::Combo("Preset", &selectedPreset, comboItems)) {
        switch (selectedPreset) {
            case 0: outRequest.width = 1280;  outRequest.height = 720;  break;
            case 1: outRequest.width = 1600;  outRequest.height = 900;  break;
            case 2: outRequest.width = 1920;  outRequest.height = 1080; break;
            case 3: outRequest.width = -1;    outRequest.height = -1;   break; // sentinel
        }
        outRequest.requested = true;
    }

    // Optional: manual size input for power users.
    static int customW = 1920;
    static int customH = 1080;
    ImGui::InputInt2("Custom", &customW);
    if (ImGui::Button("Apply Custom")) {
        outRequest.width = customW;
        outRequest.height = customH;
        outRequest.requested = true;
    }

    ImGui::End();
}

void buildLogPanel() {
    // First-use defaults: below the Debug panel, wide enough for CJK/English text.
    ImGui::SetNextWindowPos(ImVec2(20, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 300), ImGuiCond_FirstUseEver);

    ImGui::Begin("Logs");

    static bool autoScroll = true;
    static int filterLevel = 0; // 0=Debug, 1=Info, 2=Warning, 3=Error

    // -- Toolbar --
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    const char* levelItems = "Debug\0Info\0Warning\0Error\0";
    ImGui::Combo("##LevelFilter", &filterLevel, levelItems);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);

    ImGui::Separator();

    // -- Log list --
    const auto& history = Logger::instance().history();
    const LogLevel minLevel = static_cast<LogLevel>(filterLevel);

    // Count visible entries for clipper
    int visibleCount = 0;
    for (const auto& entry : history) {
        if (entry.m_level >= minLevel)
            ++visibleCount;
    }

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiListClipper clipper;
    clipper.Begin(visibleCount);
    while (clipper.Step()) {
        int lineIndex = 0;
        for (const auto& entry : history) {
            if (entry.m_level < minLevel)
                continue;

            if (lineIndex < clipper.DisplayStart) {
                ++lineIndex;
                continue;
            }
            if (lineIndex >= clipper.DisplayEnd)
                break;

            // Timestamp
            auto timeT = std::chrono::system_clock::to_time_t(entry.m_timestamp);
            std::tm tmBuf{};
#if PLATFORM_WINDOWS
            localtime_s(&tmBuf, &timeT);
#else
            localtime_r(&timeT, &tmBuf);
#endif
            char timeStr[16];
            std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

            ImGui::PushStyleColor(ImGuiCol_Text, logLevelToColor(entry.m_level));

            // Line format: [HH:MM:SS] [L] [category] function: message
            ImGui::Text("[%s] [%s] [%s] %s: %s",
                        timeStr,
                        logLevelToShortString(entry.m_level),
                        entry.m_category_name ? entry.m_category_name : "?",
                        entry.m_function ? entry.m_function : "?",
                        entry.m_message.c_str());

            ImGui::PopStyleColor();
            ++lineIndex;
        }
    }

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

void buildECSInspector(World& world, Scheduler& scheduler, f32 dt, bool& autoRun) {
    static Entity selected{0xFFFFFFFF, 0};

    // ---------- Left panel: ECS World ----------
    ImGui::SetNextWindowPos(ImVec2(20, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("ECS World");

    if (ImGui::Button("Add Entity")) {
        Entity e = world.spawn();
        world.addComponent<Position>(e, {0.0f, 0.0f});
        world.addComponent<Velocity>(e, {1.0f, 0.0f});
        world.addComponent<Health>(e, {100.0f});
        world.addComponent<NameTag>(e, {StringInternPool::instance().intern("NewEntity")});
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected") && selected.valid() && world.valid(selected)) {
        world.destroy(selected);
        selected = {0xFFFFFFFF, 0};
    }

    ImGui::Separator();
    ImGui::Checkbox("Auto Run Systems", &autoRun);
    ImGui::SameLine();
    if (ImGui::Button("Tick Once")) {
        scheduler.tick(world, dt);
    }
    ImGui::Separator();

    usize aliveCount = world.entityCount();
    ImGui::Text("Entities (%zu alive):", aliveCount);

    for (u32 id = 0; id < world.maxEntityID(); ++id) {
        Entity e{id, world.getEntityGeneration(id)};
        if (!world.valid(e)) continue;

        String label = formatString("Entity {0}", static_cast<int>(e.id));
        bool hasAny = false;
        for (const auto& pair : world.componentArrays()) {
            if (pair.second->has(e)) {
                const auto* desc = TypeRegistry::instance().findComponent(pair.first);
                if (desc) {
                    if (hasAny) {
                        label += ", ";
                    } else {
                        label += " [";
                    }
                    const char* nameResolved = Entelechy::StringInternPool::instance().resolve(desc->name);
                    if (nameResolved) label += nameResolved;
                    hasAny = true;
                }
            }
        }
        if (hasAny) label += "]";

        bool isSelected = (selected == e);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            selected = e;
        }
    }
    ImGui::End();

    // ---------- Right panel: Inspector ----------
    ImGui::SetNextWindowPos(ImVec2(360, 360), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (selected.valid() && world.valid(selected)) {
        ImGui::Text("Entity ID: %u, Generation: %u", selected.id, selected.generation);
        ImGui::Separator();

        for (const auto& pair : world.componentArrays()) {
            if (!pair.second->has(selected)) continue;
            const auto* desc = TypeRegistry::instance().findComponent(pair.first);
            if (!desc) continue;
            void* raw = const_cast<void*>(pair.second->getRaw(selected));
            if (!raw) continue;

            const char* nameResolved = Entelechy::StringInternPool::instance().resolve(desc->name);
            if (ImGui::TreeNodeEx(nameResolved ? nameResolved : "", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool componentChanged = false;
                for (const auto& field : desc->fields) {
                    if (drawField(field, raw)) componentChanged = true;
                }
                ImGui::TreePop();
                // If the Transform component was edited, mark it dirty so the
                // TransformPropagationSystem recomputes the world matrix.
                if (componentChanged && desc->name == "Transform"_sid) {
                    if (auto* trans = world.getComponent<Entelechy::Transform>(selected)) {
                        trans->dirty = 1;
                    }
                }
            }
        }
    } else {
        ImGui::TextDisabled("No entity selected.");
    }
    ImGui::End();
}

} // namespace Entelechy
