cmake_minimum_required(VERSION 3.20)

add_library(CoreTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_world.cpp
)

target_link_libraries(CoreTests PUBLIC CoreLib TestFrameworkLib)

target_compile_features(CoreTests PUBLIC cxx_std_20)
