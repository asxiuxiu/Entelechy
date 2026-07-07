cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME ThreadPoolTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_thread_pool.cpp
    PUBLIC_DEPS
        ThreadPoolLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(ThreadPoolTests)
