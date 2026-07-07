cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME RenderTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_render_view_resources.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_sort_key.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_binned_render_phase.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_render_parallel.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_gpu_resource_lifecycle.cpp
    PUBLIC_DEPS
        RenderLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(RenderTests)
