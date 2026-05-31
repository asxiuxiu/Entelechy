# EntelechyModule.cmake
# Unified module creation macros for the Entelechy build system.
# Phase 3: public/private directory boundary with module-prefix includes.

function(entelechy_module)
    cmake_parse_arguments(MOD
        "NO_TESTS"
        "NAME;TYPE;INIT_FUNCTION;NAMESPACE"
        "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS"
        ${ARGN}
    )

    # ---- Validation ----
    if(NOT MOD_NAME)
        message(FATAL_ERROR "entelechy_module: NAME is required")
    endif()

    # ---- Defaults ----
    if(NOT MOD_TYPE)
        set(MOD_TYPE STATIC)
    endif()
    if(NOT MOD_NAMESPACE)
        set(MOD_NAMESPACE Entelechy)
    endif()

    # ---- Auto-discover sources and public/private dirs ----
    set(_HAS_PUBLIC FALSE)
    set(_HAS_PRIVATE FALSE)
    set(_AUTO_SOURCES "")

    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/public")
        set(_HAS_PUBLIC TRUE)
        file(GLOB_RECURSE _PUBLIC_HEADERS
            "${CMAKE_CURRENT_LIST_DIR}/public/*.h"
            "${CMAKE_CURRENT_LIST_DIR}/public/*.hpp"
        )
        list(APPEND _AUTO_SOURCES ${_PUBLIC_HEADERS})
    endif()
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/private")
        set(_HAS_PRIVATE TRUE)
        file(GLOB_RECURSE _PRIVATE_SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/private/*.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/private/*.h"
            "${CMAKE_CURRENT_LIST_DIR}/private/*.hpp"
        )
        list(APPEND _AUTO_SOURCES ${_PRIVATE_SOURCES})
    endif()

    if(NOT MOD_SOURCES)
        set(MOD_SOURCES ${_AUTO_SOURCES})
        if(NOT MOD_SOURCES)
            message(FATAL_ERROR "entelechy_module: No sources found for ${MOD_NAME}. "
                "Provide SOURCES explicitly, or create public/ and/or private/ directories.")
        endif()
    else()
        # If SOURCES is explicitly provided, still append auto-discovered files
        # so modules with external sources (e.g. imgui backends) don't lose their own code.
        list(APPEND MOD_SOURCES ${_AUTO_SOURCES})
    endif()

    # ---- Create target ----
    if(MOD_TYPE STREQUAL "EXECUTABLE")
        add_executable(${MOD_NAME} ${MOD_SOURCES})
    else()
        add_library(${MOD_NAME} ${MOD_TYPE} ${MOD_SOURCES})
    endif()

    # ---- C++ standard ----
    target_compile_features(${MOD_NAME} PUBLIC cxx_std_20)

    # ---- Include directories ----
    # Phase 3: public/private boundary with module-prefix includes via staging links.
    # Physical layout: <module>/public/event/...
    # Staging link:    build/include/<module>/ -> <module>/public/
    # Include path:    build/include/
    # Usage:           #include "<module>/event/..."
    if(_HAS_PUBLIC)
        set(_STAGE_DIR ${CMAKE_BINARY_DIR}/include)
        # Link name must match the directory name used in #include paths (e.g. "ecs/event/...")
        # rather than the CMake target name (e.g. "EcsLib").
        get_filename_component(_MODULE_DIR_NAME "${CMAKE_CURRENT_LIST_DIR}" NAME)
        set(_MODULE_LINK ${_STAGE_DIR}/${_MODULE_DIR_NAME})
        set(_PUBLIC_DIR "${CMAKE_CURRENT_LIST_DIR}/public")

        file(MAKE_DIRECTORY ${_STAGE_DIR})

        if(WIN32)
            # Windows: use junction point (no admin privilege needed)
            if(NOT EXISTS ${_MODULE_LINK})
                file(TO_NATIVE_PATH "${_PUBLIC_DIR}" _TARGET_NATIVE)
                file(TO_NATIVE_PATH "${_MODULE_LINK}" _LINK_NATIVE)
                execute_process(
                    COMMAND cmd /c mklink /J "${_LINK_NATIVE}" "${_TARGET_NATIVE}"
                    RESULT_VARIABLE _link_result
                    ERROR_VARIABLE _link_error
                    OUTPUT_QUIET
                )
                if(NOT _link_result EQUAL 0)
                    message(FATAL_ERROR "Failed to create junction for ${_MODULE_DIR_NAME}: ${_link_error}")
                endif()
            endif()
        else()
            # Unix-like: use symbolic link
            if(NOT EXISTS ${_MODULE_LINK})
                file(CREATE_LINK "${_PUBLIC_DIR}" "${_MODULE_LINK}" SYMBOLIC)
            endif()
        endif()

        target_include_directories(${MOD_NAME} PUBLIC ${_STAGE_DIR})
    else()
        # Fallback: legacy layout — expose module root for un-migrated modules
        target_include_directories(${MOD_NAME} PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}
        )
    endif()

    if(_HAS_PRIVATE)
        target_include_directories(${MOD_NAME} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/private
        )
    endif()

    # Transition compatibility: expose parent directory as PRIVATE for internal includes
    # that haven't been migrated to module-prefix style yet.
    # TODO(Phase 3+): Remove once all internal relative includes are eliminated.
    target_include_directories(${MOD_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/..
    )

    # ---- Dependencies ----
    if(MOD_PUBLIC_DEPS)
        target_link_libraries(${MOD_NAME} PUBLIC ${MOD_PUBLIC_DEPS})
    endif()
    if(MOD_PRIVATE_DEPS)
        target_link_libraries(${MOD_NAME} PRIVATE ${MOD_PRIVATE_DEPS})
    endif()

    # ---- Global registration ----
    set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES ${MOD_NAME})

    if(MOD_INIT_FUNCTION)
        set_property(GLOBAL APPEND PROPERTY ENTELECHY_INIT_FUNCTIONS
            "${MOD_NAMESPACE}::${MOD_INIT_FUNCTION}"
        )
    endif()

    # ---- Automatic test subdirectory ----
    if(NOT MOD_NO_TESTS AND NOT SHIPPING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests")
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
    endif()
endfunction()

function(entelechy_get_enabled_modules out_var)
    get_property(_modules GLOBAL PROPERTY ENTELECHY_ENABLED_MODULES)
    set(${out_var} ${_modules} PARENT_SCOPE)
endfunction()

function(entelechy_get_init_functions out_var)
    get_property(_funcs GLOBAL PROPERTY ENTELECHY_INIT_FUNCTIONS)
    set(${out_var} ${_funcs} PARENT_SCOPE)
endfunction()
