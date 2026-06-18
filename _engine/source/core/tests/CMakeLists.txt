cmake_minimum_required(VERSION 3.20)

add_library(CoreTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_string.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_memory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_vec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_mat4.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_quat.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_geometry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_misc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_radix_sort.cpp
)

target_link_libraries(CoreTests PUBLIC CoreLib TestFrameworkLib)

target_include_directories(CoreTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../private)

target_compile_features(CoreTests PUBLIC cxx_std_20)

# Register with the global module list so test_runner can discover us
set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES CoreTests)
