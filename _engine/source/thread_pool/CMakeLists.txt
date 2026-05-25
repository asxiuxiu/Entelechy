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

if(NOT SHIPPING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
