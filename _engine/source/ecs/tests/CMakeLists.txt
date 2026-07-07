cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME EcsTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_world.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_archetype_world.cpp
    PUBLIC_DEPS
        EcsLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(EcsTests)
