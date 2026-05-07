cmake_minimum_required(VERSION 3.20)

add_library(MemoryLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/frame_arena.cpp
    ${CMAKE_CURRENT_LIST_DIR}/frame_arena.h
    ${CMAKE_CURRENT_LIST_DIR}/object_pool.h
    ${CMAKE_CURRENT_LIST_DIR}/dynamic_array.h
    ${CMAKE_CURRENT_LIST_DIR}/allocator.h
)

target_include_directories(MemoryLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(MemoryLib PUBLIC FoundationLib)

target_compile_features(MemoryLib PUBLIC cxx_std_20)
