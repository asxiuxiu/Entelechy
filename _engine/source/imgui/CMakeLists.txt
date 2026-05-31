cmake_minimum_required(VERSION 3.20)

find_package(imgui REQUIRED)
find_package(glad REQUIRED)
find_package(glfw3 REQUIRED)

# Conan imgui package places backends under res/bindings.
# For multi-config generators or Debug-only builds, the package folder variable
# may be suffixed with _DEBUG instead of _RELEASE.
if(DEFINED imgui_PACKAGE_FOLDER_RELEASE)
    set(IMGUI_PACKAGE_DIR ${imgui_PACKAGE_FOLDER_RELEASE})
elseif(DEFINED imgui_PACKAGE_FOLDER_DEBUG)
    set(IMGUI_PACKAGE_DIR ${imgui_PACKAGE_FOLDER_DEBUG})
else()
    message(FATAL_ERROR "Could not determine imgui package folder. "
                        "Make sure imgui is installed via Conan.")
endif()
set(IMGUI_BACKENDS_DIR ${IMGUI_PACKAGE_DIR}/res/bindings)

entelechy_module(
    NAME ImGuiLib
    SOURCES
        ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
    PUBLIC_DEPS
        imgui::imgui
        glad::glad
        glfw
        WindowLib
        LogLib
        CoreLib
        EcsLib
    INIT_FUNCTION initImGui
    NAMESPACE Entelechy
)


target_include_directories(ImGuiLib PUBLIC ${IMGUI_BACKENDS_DIR})
