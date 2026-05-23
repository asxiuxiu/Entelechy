cmake_minimum_required(VERSION 3.20)

add_library(VFSLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/vfs_types.h
    ${CMAKE_CURRENT_LIST_DIR}/mount_point.h
    ${CMAKE_CURRENT_LIST_DIR}/mount_point.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vfs.h
    ${CMAKE_CURRENT_LIST_DIR}/vfs.cpp
)

target_include_directories(VFSLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(VFSLib PUBLIC BaseLib)

target_compile_features(VFSLib PUBLIC cxx_std_20)
