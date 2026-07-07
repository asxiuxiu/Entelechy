#pragma once

struct GLFWwindow;

namespace Entelechy
{

// Lightweight wrapper around Dear ImGui lifecycle.
// Manages context creation, backend initialization, and per-frame calls.
// ImGui itself remains global-state driven; this class only provides
// a structured entry point for the engine's main loop.
class ImGuiManager
{
public:
    ImGuiManager();
    ~ImGuiManager();

    // Initialize ImGui context + GLFW + OpenGL3 backends.
    // Must be called after GLFW window and OpenGL context are ready.
    bool init(GLFWwindow *window);

    // Shutdown backends and destroy context.
    void shutdown();

    // Begin a new ImGui frame. Call after glfwPollEvents().
    void newFrame();

    // Generate draw data from all ImGui::Begin/End blocks.
    // Call after all UI building code.
    void render();

    // Submit ImGui draw data to OpenGL. Call after 3D scene rendering, before swapBuffers.
    void renderDrawData();

    // Handle multi-viewport windows (ImGui windows dragged outside main window).
    // Call after renderDrawData().
    void updatePlatformWindows();

    bool isInitialized() const
    {
        return m_initialized;
    }

private:
    GLFWwindow *m_window = nullptr;
    bool m_initialized = false;
};

} // namespace Entelechy
