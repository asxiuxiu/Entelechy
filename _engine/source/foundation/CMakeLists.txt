cmake_minimum_required(VERSION 3.20)

add_library(FoundationLib INTERFACE)

target_include_directories(FoundationLib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(FoundationLib INTERFACE cxx_std_20)
