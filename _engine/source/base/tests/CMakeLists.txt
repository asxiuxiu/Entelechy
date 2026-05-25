cmake_minimum_required(VERSION 3.20)

add_library(BaseTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_small_string.cpp
)

target_link_libraries(BaseTests PUBLIC BaseLib TestFrameworkLib)

target_compile_features(BaseTests PUBLIC cxx_std_20)
