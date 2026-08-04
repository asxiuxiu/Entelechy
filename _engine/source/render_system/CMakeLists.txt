cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME RenderSystemLib
    PUBLIC_DEPS
        RenderCoreLib
        EcsLib
        CoreLib
    PRIVATE_DEPS
        ThreadPoolLib
        LogLib
)
