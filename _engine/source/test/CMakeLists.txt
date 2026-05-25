cmake_minimum_required(VERSION 3.20)

add_library(TestFrameworkLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/test_framework.h
    ${CMAKE_CURRENT_LIST_DIR}/test_framework.cpp
)

target_include_directories(TestFrameworkLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(TestFrameworkLib PUBLIC BaseLib)

target_compile_features(TestFrameworkLib PUBLIC cxx_std_20)
