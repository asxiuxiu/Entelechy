cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME AssetTests
    TYPE OBJECT
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/test_asset.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_asset_types.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_mesh_asset_loader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/test_material_asset_loader.cpp
    PUBLIC_DEPS
        AssetLib
        TestFrameworkLib
    NO_TESTS
)

entelechy_module_parent_private_include(AssetTests)
