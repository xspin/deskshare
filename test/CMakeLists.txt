enable_testing()

include_directories(../src)

find_package(GTest REQUIRED)

add_executable(test_all
    test_utils.cpp
    test_capture.cpp
)

target_link_libraries(test_all PRIVATE
    unittest
    GTest::gtest_main
)

# 注册测试（让 CMake 识别该测试，支持 ctest 命令）
include(GoogleTest)
gtest_discover_tests(test_all)
