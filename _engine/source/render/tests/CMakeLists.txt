cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)

entelechy_module(
    NAME RenderCoreTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_gpu_resource_lifecycle.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_command_buffer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_d3d12_backend.cpp
    PUBLIC_DEPS
        RenderCoreLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(RenderCoreTests)
