cmake_minimum_required(VERSION 3.20)

add_library(MathLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/math_lib.cpp
    ${CMAKE_CURRENT_LIST_DIR}/math_lib.h
    ${CMAKE_CURRENT_LIST_DIR}/math_config.h
    ${CMAKE_CURRENT_LIST_DIR}/align.h
    ${CMAKE_CURRENT_LIST_DIR}/vec.h
    ${CMAKE_CURRENT_LIST_DIR}/quat.h
    ${CMAKE_CURRENT_LIST_DIR}/mat4.h
    ${CMAKE_CURRENT_LIST_DIR}/aabb.h
    ${CMAKE_CURRENT_LIST_DIR}/random.h
    ${CMAKE_CURRENT_LIST_DIR}/simd.h
    ${CMAKE_CURRENT_LIST_DIR}/half.h
    ${CMAKE_CURRENT_LIST_DIR}/ray.h
    ${CMAKE_CURRENT_LIST_DIR}/frustum.h
    ${CMAKE_CURRENT_LIST_DIR}/transform_component.h
    ${CMAKE_CURRENT_LIST_DIR}/transform_propagation_system.h
    ${CMAKE_CURRENT_LIST_DIR}/transform_propagation_system.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hierarchy.h
)

target_include_directories(MathLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(MathLib PUBLIC BaseLib CoreLib)

target_compile_features(MathLib PUBLIC cxx_std_20)

if(NOT SHIPPING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()

# Float determinism: avoid aggressive fast-math that breaks cross-platform consistency
if (MSVC)
    target_compile_options(MathLib PRIVATE /fp:precise)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Do not add -ffast-math by default
endif()

# Optional strict-FP switch (disable FMA auto-contraction)
option(ENTELECHY_MATH_STRICT_FP "Disable FMA and floating-point contraction for deterministic math" OFF)
if (ENTELECHY_MATH_STRICT_FP)
    if (MSVC)
        target_compile_options(MathLib PRIVATE /fp:strict)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(MathLib PRIVATE -ffp-contract=off)
    endif()
endif()
