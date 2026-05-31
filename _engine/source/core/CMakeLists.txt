cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME CoreLib
)


# Additional include paths for subdirectories inside CoreLib
# Float determinism: avoid aggressive fast-math that breaks cross-platform consistency
if(MSVC)
    target_compile_options(CoreLib PRIVATE /fp:precise)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Do not add -ffast-math by default
endif()

option(ENTELECHY_MATH_STRICT_FP "Disable FMA and floating-point contraction for deterministic math" OFF)
if(ENTELECHY_MATH_STRICT_FP)
    if(MSVC)
        target_compile_options(CoreLib PRIVATE /fp:strict)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(CoreLib PRIVATE -ffp-contract=off)
    endif()
endif()
