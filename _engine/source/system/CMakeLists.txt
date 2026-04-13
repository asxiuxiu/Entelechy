cmake_minimum_required(VERSION 3.20)

add_library(SystemLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/scheduler.h
    ${CMAKE_CURRENT_LIST_DIR}/system_init.cpp
    ${CMAKE_CURRENT_LIST_DIR}/movement_system.h
    ${CMAKE_CURRENT_LIST_DIR}/movement_system.cpp
)

target_include_directories(SystemLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(SystemLib PUBLIC cxx_std_17)

target_link_libraries(SystemLib PUBLIC
    CoreLib
)
