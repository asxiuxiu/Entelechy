cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME EditorLib
    PUBLIC_DEPS
        ImGuiLib
        EcsLib
        CoreLib
    INIT_FUNCTION initEditor
    NAMESPACE Entelechy
)
