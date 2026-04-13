cmake_minimum_required(VERSION 3.20)

add_library(CoreLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/world.cpp
    ${CMAKE_CURRENT_LIST_DIR}/world.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.h
)

target_include_directories(CoreLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(CoreLib PUBLIC cxx_std_17)
