cmake_minimum_required(VERSION 3.20)

add_library(AssetLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/asset_types.h
    ${CMAKE_CURRENT_LIST_DIR}/asset_handle.h
    ${CMAKE_CURRENT_LIST_DIR}/handle_table.h
    ${CMAKE_CURRENT_LIST_DIR}/assets.h
    ${CMAKE_CURRENT_LIST_DIR}/asset_loader.h
    ${CMAKE_CURRENT_LIST_DIR}/asset_server.h
    ${CMAKE_CURRENT_LIST_DIR}/asset_server.cpp
)

target_include_directories(AssetLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(AssetLib PUBLIC BaseLib VFSLib)

target_compile_features(AssetLib PUBLIC cxx_std_20)

if(NOT SHIPPING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
