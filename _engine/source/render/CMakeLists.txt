cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)
find_package(stb REQUIRED)

entelechy_module(
    NAME RenderCoreLib
    PUBLIC_DEPS
        glad::glad
        WindowLib
        LogLib
        CoreLib
    PRIVATE_DEPS
        ThreadPoolLib
        stb::stb
)

