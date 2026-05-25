cmake_minimum_required(VERSION 3.20)

set(POSSIBLE_TEST_TARGETS
    BaseTests
    CoreTests
    MotorTests
    BridgeTests
    MathTests
    MemoryTests
    WindowTests
    LogTests
    RenderTests
    ImGuiTests
    VFSTests
    AssetTests
    ThreadPoolTests
    RuntimeTests
)

set(test_objects "")
foreach(target ${POSSIBLE_TEST_TARGETS})
    if(TARGET ${target})
        list(APPEND test_objects "$<TARGET_OBJECTS:${target}>")
    endif()
endforeach()

add_executable(EntelechyTests
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${test_objects}
)

target_include_directories(EntelechyTests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
)

set(ENTELECHY_TEST_LIBS
    TestFrameworkLib
    BaseLib
    CoreLib
    MotorLib
    BridgeLib
    MathLib
    MemoryLib
    WindowLib
    LogLib
    RenderLib
    ImGuiLib
    VFSLib
    AssetLib
    ThreadPoolLib
)

if(TARGET RuntimeLib)
    list(APPEND ENTELECHY_TEST_LIBS RuntimeLib)
endif()

target_link_libraries(EntelechyTests PRIVATE ${ENTELECHY_TEST_LIBS})

target_compile_features(EntelechyTests PUBLIC cxx_std_20)
