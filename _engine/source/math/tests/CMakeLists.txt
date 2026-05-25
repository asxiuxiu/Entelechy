cmake_minimum_required(VERSION 3.20)

add_library(MathTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_vec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_mat4.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_quat.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_geometry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_misc.cpp
)

target_link_libraries(MathTests PUBLIC MathLib TestFrameworkLib)

target_compile_features(MathTests PUBLIC cxx_std_20)
