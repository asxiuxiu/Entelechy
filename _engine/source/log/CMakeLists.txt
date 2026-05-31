cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME LogLib
    PUBLIC_DEPS
        CoreLib
)


if(MSVC)
    target_compile_definitions(LogLib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
