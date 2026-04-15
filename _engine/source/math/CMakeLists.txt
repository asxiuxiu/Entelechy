cmake_minimum_required(VERSION 3.20)

add_library(MathLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/math_lib.cpp
    ${CMAKE_CURRENT_LIST_DIR}/math_lib.h
    ${CMAKE_CURRENT_LIST_DIR}/align.h
    ${CMAKE_CURRENT_LIST_DIR}/vec.h
    ${CMAKE_CURRENT_LIST_DIR}/quat.h
    ${CMAKE_CURRENT_LIST_DIR}/mat4.h
    ${CMAKE_CURRENT_LIST_DIR}/aabb.h
    ${CMAKE_CURRENT_LIST_DIR}/random.h
)

target_include_directories(MathLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(MathLib PUBLIC cxx_std_17)
