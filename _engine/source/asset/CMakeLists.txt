cmake_minimum_required(VERSION 3.20)

find_package(stb REQUIRED)

entelechy_module(
    NAME AssetLib
    PUBLIC_DEPS
        CoreLib
        VFSLib
    PRIVATE_DEPS
        LogLib
        stb::stb
)

