cmake_minimum_required(VERSION 3.20)

add_library(CoreLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/world.cpp
    ${CMAKE_CURRENT_LIST_DIR}/world.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
    ${CMAKE_CURRENT_LIST_DIR}/entity_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/entity_registry.h
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.h
    ${CMAKE_CURRENT_LIST_DIR}/atom_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/atom_registry.h
    ${CMAKE_CURRENT_LIST_DIR}/component_array.h
    ${CMAKE_CURRENT_LIST_DIR}/sparse_set.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sparse_set.h
    ${CMAKE_CURRENT_LIST_DIR}/archetype_chunk.h
    ${CMAKE_CURRENT_LIST_DIR}/command_buffer.h
    ${CMAKE_CURRENT_LIST_DIR}/query.h
    ${CMAKE_CURRENT_LIST_DIR}/scheduler.h
    ${CMAKE_CURRENT_LIST_DIR}/scheduler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/phase.h
    ${CMAKE_CURRENT_LIST_DIR}/system_desc.h
    ${CMAKE_CURRENT_LIST_DIR}/events.h
    ${CMAKE_CURRENT_LIST_DIR}/event_lifetime.h
    ${CMAKE_CURRENT_LIST_DIR}/event_cleanup_system.h
    ${CMAKE_CURRENT_LIST_DIR}/event_cleanup_system.cpp
    ${CMAKE_CURRENT_LIST_DIR}/event_buffer.h
    ${CMAKE_CURRENT_LIST_DIR}/plugin.h
    ${CMAKE_CURRENT_LIST_DIR}/app.h
    ${CMAKE_CURRENT_LIST_DIR}/app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/prefab.h
    ${CMAKE_CURRENT_LIST_DIR}/prefab.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scene_serializer.h
    ${CMAKE_CURRENT_LIST_DIR}/scene_serializer.cpp
)

target_include_directories(CoreLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(CoreLib PUBLIC MemoryLib)

target_compile_features(CoreLib PUBLIC cxx_std_20)
