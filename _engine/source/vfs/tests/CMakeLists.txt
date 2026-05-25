cmake_minimum_required(VERSION 3.20)

add_library(VFSTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_vfs.cpp
)

target_link_libraries(VFSTests PUBLIC VFSLib TestFrameworkLib)

target_compile_features(VFSTests PUBLIC cxx_std_20)
