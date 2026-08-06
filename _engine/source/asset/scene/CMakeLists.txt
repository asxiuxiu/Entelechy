cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME SceneLib
    PUBLIC_DEPS
        CoreLib
        AssetLib
        EcsLib
        RenderSystemLib
    PRIVATE_DEPS
        LogLib
)
