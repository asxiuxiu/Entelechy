cmake_minimum_required(VERSION 3.20)

add_executable(TestBatch0
    ${CMAKE_CURRENT_LIST_DIR}/test_batch0.cpp
)

target_include_directories(TestBatch0 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(TestBatch0 PRIVATE
    CoreLib
    VFSLib
    ThreadPoolLib
    BaseLib
    LogLib
)

target_compile_features(TestBatch0 PUBLIC cxx_std_20)
