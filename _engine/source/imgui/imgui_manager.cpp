#include "imgui_manager.h"
#include "log/log_macros.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace Entelechy {

ImGuiManager::ImGuiManager() = default;

ImGuiManager::~ImGuiManager() {
    if (m_initialized) {
        shutdown();
    }
}

bool ImGuiManager::init(GLFWwindow* window) {
    if (!window) {
        LOG_ERROR(LogCategories::kEngine, "ImGuiManager::init: window is null");
        return false;
    }
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Enable Docking for in-window panel snapping/splitting.
    // Multi-Viewport is kept off for now so panels stay inside the game
    // window and don't float as independent OS windows on first launch.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Auto-save window layout to imgui.ini on disk.
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();

    // Apply DPI scaling based on the monitor content scale.
    // This ensures readable UI on HiDPI / Retina displays.
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float scale = (xscale > yscale) ? xscale : yscale;
    if (scale > 1.0f) {
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = scale;
        ImGui::GetStyle().ScaleAllSizes(scale);
        LOG_INFO(LogCategories::kEngine, "HiDPI detected (scale=%.2f); ImGui UI scaled accordingly", scale);
    }

    // Initialize Platform + Renderer backends.
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        LOG_ERROR(LogCategories::kEngine, "ImGui_ImplGlfw_InitForOpenGL failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 460")) {
        LOG_ERROR(LogCategories::kEngine, "ImGui_ImplOpenGL3_Init failed");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    LOG_INFO(LogCategories::kEngine, "ImGui initialized (Docking + Viewports)");
    m_initialized = true;
    return true;
}

void ImGuiManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_window = nullptr;
    m_initialized = false;

    LOG_INFO(LogCategories::kEngine, "ImGui shutdown");
}

void ImGuiManager::newFrame() {
    if (!m_initialized) {
        return;
    }
    // Order matters: renderer newFrame first, then platform, then ImGui.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::render() {
    if (!m_initialized) {
        return;
    }
    ImGui::Render();
}

void ImGuiManager::renderDrawData() {
    if (!m_initialized) {
        return;
    }
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::updatePlatformWindows() {
    if (!m_initialized) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}

} // namespace Entelechy
