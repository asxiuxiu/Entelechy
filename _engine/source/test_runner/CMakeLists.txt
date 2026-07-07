cmake_minimum_required(VERSION 3.20)

entelechy_get_enabled_modules(ALL_MODULES)

set(TEST_OBJECTS "")
set(TEST_TARGETS "")
foreach(TARGET_NAME ${ALL_MODULES})
    if(TARGET_NAME MATCHES "Tests$")
        list(APPEND TEST_OBJECTS "$<TARGET_OBJECTS:${TARGET_NAME}>")
        list(APPEND TEST_TARGETS ${TARGET_NAME})
    endif()
endforeach()

# Collect all non-test libraries
set(TEST_LIBS "")
foreach(TARGET_NAME ${ALL_MODULES})
    if(NOT TARGET_NAME MATCHES "Tests$" AND NOT TARGET_NAME STREQUAL "Entelechy")
        list(APPEND TEST_LIBS ${TARGET_NAME})
    endif()
endforeach()

entelechy_module(
    NAME EntelechyTests
    TYPE EXECUTABLE
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/private/main.cpp
        ${TEST_OBJECTS}
    PRIVATE_DEPS
        ${TEST_LIBS}
        ${TEST_TARGETS}
    NO_TESTS
)

set_target_properties(EntelechyTests PROPERTIES FOLDER "Tests")

