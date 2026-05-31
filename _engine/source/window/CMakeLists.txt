cmake_minimum_required(VERSION 3.20)

find_package(glfw3 REQUIRED)

entelechy_module(
    NAME WindowLib
    PUBLIC_DEPS
        glfw
        CoreLib
        LogLib
    INIT_FUNCTION initWindow
    NAMESPACE Entelechy
)

