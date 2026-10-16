set(CAP_SRC
    capturer.cpp
)

if (DARWIN)
    list(APPEND CAP_SRC
        darwin/ScreenCaptureImpl.mm
        darwin/ScreenCapture.cpp
        darwin/FrameProcessor.cpp
    )

    set(CAP_LIBS 
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework CoreVideo"
    )
elseif (WIN32)
    list(APPEND CAP_SRC
        windows/capturer_win.cpp
    )

endif()

add_library(capture STATIC ${CAP_SRC})

target_link_libraries(capture PRIVATE ${CAP_LIBS})

target_include_directories(capture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

