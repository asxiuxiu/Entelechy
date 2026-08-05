cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME RuntimeLib
    PUBLIC_DEPS
        EcsLib
        BridgeLib
        RenderSystemLib
        WindowLib
        AssetLib
        LogLib
    INIT_FUNCTION initRuntime
    NAMESPACE game
)

