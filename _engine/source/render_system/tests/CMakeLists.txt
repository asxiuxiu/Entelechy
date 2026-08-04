cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME RenderSystemTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_render_parallel.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_render_view_resources.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_binned_render_phase.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_sort_key.cpp
    PUBLIC_DEPS
        RenderSystemLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(RenderSystemTests)
