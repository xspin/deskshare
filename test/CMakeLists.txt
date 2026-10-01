enable_testing()

include_directories(../src)

find_package(GTest REQUIRED)

set(TEST_LIBS unittest GTest::gtest_main)

add_executable(test_utils test_utils.cpp test_capture.cpp)
target_link_libraries(test_utils PRIVATE ${TEST_LIBS})


add_executable(test_perf_capture test_perf_capture.cpp)
target_link_libraries(test_perf_capture PRIVATE ${TEST_LIBS})


add_executable(test_perf_rgba test_perf_rgba.cpp)
target_link_libraries(test_perf_rgba PRIVATE ${TEST_LIBS})



include(GoogleTest)
gtest_discover_tests(test_utils test_perf_capture test_perf_rgba)
