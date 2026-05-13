cmake_minimum_required(VERSION 3.20)

add_library(MotorLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/movement_system.h
    ${CMAKE_CURRENT_LIST_DIR}/movement_system.cpp
)

target_include_directories(MotorLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(MotorLib PUBLIC CoreLib)

target_compile_features(MotorLib PUBLIC cxx_std_20)
