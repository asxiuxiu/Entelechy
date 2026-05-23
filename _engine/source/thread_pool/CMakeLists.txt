cmake_minimum_required(VERSION 3.20)

add_library(ThreadPoolLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.h
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.cpp
)

target_include_directories(ThreadPoolLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ThreadPoolLib PUBLIC BaseLib)

target_compile_features(ThreadPoolLib PUBLIC cxx_std_20)
