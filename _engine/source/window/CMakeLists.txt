cmake_minimum_required(VERSION 3.20)

find_package(glfw3 REQUIRED)

add_library(WindowLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/window_init.cpp
    ${CMAKE_CURRENT_LIST_DIR}/glfw_window.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input_queue.cpp
)

target_include_directories(WindowLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(WindowLib PUBLIC cxx_std_17)

target_link_libraries(WindowLib PUBLIC
    glfw
    CoreLib
)
