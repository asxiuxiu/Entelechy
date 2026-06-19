cmake_minimum_required(VERSION 3.20)

add_library(RenderTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_render_view_resources.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_sort_key.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_binned_render_phase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_render_parallel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_gpu_resource_lifecycle.cpp
)

target_link_libraries(RenderTests PUBLIC RenderLib TestFrameworkLib)

target_include_directories(RenderTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../private)

target_compile_features(RenderTests PUBLIC cxx_std_20)

set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES RenderTests)
