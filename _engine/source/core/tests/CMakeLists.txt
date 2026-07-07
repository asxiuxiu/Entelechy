cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME CoreTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_string.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_memory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_vec.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_mat4.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_quat.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_geometry.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_misc.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_radix_sort.cpp
    PUBLIC_DEPS
        CoreLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(CoreTests)
