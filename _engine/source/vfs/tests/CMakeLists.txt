cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME VFSTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_vfs.cpp
    PUBLIC_DEPS
        VFSLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(VFSTests)
