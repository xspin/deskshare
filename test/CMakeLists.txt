enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/capture)

find_package(GTest REQUIRED)

add_library(unittest STATIC
   ${CMAKE_SOURCE_DIR}/src/h264/h264.cpp
   ${CMAKE_SOURCE_DIR}/src/utils.h
   ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

target_link_libraries(unittest PRIVATE
    ${LIBS} capture
)

set(TEST_LIBS unittest GTest::gtest_main)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils PRIVATE ${TEST_LIBS})

add_executable(test_capture test_capture.cpp)
target_link_libraries(test_capture PRIVATE ${TEST_LIBS})

if(APPLE AND IGNORE)
set_target_properties(test_capture PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
)

add_custom_command(TARGET test_capture POST_BUILD
    COMMAND codesign --force --deep --sign "DeskShareDev" 
                $<TARGET_BUNDLE_DIR:test_capture>
        COMMENT "Signing test_capture.app"
    )
endif()


add_executable(test_perf_jpeg test_perf_jpeg.cpp)
target_link_libraries(test_perf_jpeg PRIVATE ${TEST_LIBS})


add_executable(test_perf_rgba test_perf_rgba.cpp)
target_link_libraries(test_perf_rgba PRIVATE ${TEST_LIBS})

add_executable(test_h264 test_h264.cpp)
target_link_libraries(test_h264 PRIVATE ${TEST_LIBS})


include(GoogleTest)
gtest_discover_tests(test_utils test_capture)
