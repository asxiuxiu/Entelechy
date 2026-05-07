cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)

add_library(RenderLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/opengl_backend.cpp
)

target_include_directories(RenderLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_compile_features(RenderLib PUBLIC cxx_std_20)

target_link_libraries(RenderLib PUBLIC
    glad::glad
    WindowLib
    LogLib
)
