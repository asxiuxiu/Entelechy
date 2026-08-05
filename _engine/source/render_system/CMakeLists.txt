cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME RenderSystemLib
    PUBLIC_DEPS
        RenderCoreLib
        EcsLib
        CoreLib
        AssetLib
    PRIVATE_DEPS
        ThreadPoolLib
        LogLib
)
