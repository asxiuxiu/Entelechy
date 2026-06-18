cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)

entelechy_module(
    NAME RenderLib
    PUBLIC_DEPS
        glad::glad
        WindowLib
        LogLib
        CoreLib
        EcsLib
    PRIVATE_DEPS
        ThreadPoolLib
)

