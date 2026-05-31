cmake_minimum_required(VERSION 3.20)

add_library(AssetTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_asset.cpp
)

target_link_libraries(AssetTests PUBLIC AssetLib TestFrameworkLib)

target_include_directories(AssetTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../private)

target_compile_features(AssetTests PUBLIC cxx_std_20)

set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES AssetTests)
