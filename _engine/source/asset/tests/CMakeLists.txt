cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME AssetTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_asset.cpp
    PUBLIC_DEPS
        AssetLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(AssetTests)
