cmake_minimum_required(VERSION 3.10)

project(deskshare VERSION 1.2.0)
message("Project: ${PROJECT_NAME} ${PROJECT_VERSION}")

# WIN32 / UNIX / DARWIN 
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(DARWIN TRUE)
endif()

message("System Name: ${CMAKE_SYSTEM_NAME}")
message("System Version: ${CMAKE_SYSTEM_VERSION}")
message("System Processor: ${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "COmpiler version: ${CMAKE_CXX_COMPILER_VERSION}")

option(ENABLE_ASAN "Enable Address Sanitizer for memory leak detection" OFF)
option(ENABLE_DEBUG "Enable Debug compile" OFF)
option(ENABLE_GUI "Enable GUI build" ON)
option(ENABLE_TEST "Enable unit test" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


if (WIN32)
    set(LIBUV_ROOT_DIR "C:/libuv-1.51.0")
    set(LIBUV_LIB_DIR "${LIBUV_ROOT_DIR}/build")

    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/x64/MT")

    set(FLTK_ROOT_DIR "C:/fltk-1.4.4")
    set(FLTK_LIB_DIR "${FLTK_ROOT_DIR}/build/lib")
    set(FLTK_INCLUDE_DIR "${FLTK_ROOT_DIR}")

elseif (DARWIN)
    set(LIBUV_ROOT_DIR "/usr/local/opt/libuv")
    set(LIBUV_LIB_DIR "${LIBUV_ROOT_DIR}/lib")

    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib")

    set(FLTK_ROOT_DIR "/usr/local/opt/fltk")
    set(FLTK_LIB_DIR "${FLTK_ROOT_DIR}/lib")
    set(FLTK_INCLUDE_DIR "${FLTK_ROOT_DIR}/include")
endif()

include_directories(${LIBUV_ROOT_DIR}/include)
include_directories(${OPENSSL_ROOT_DIR}/include)
include_directories(${FLTK_INCLUDE_DIR})

find_library(LIBUV
    NAMES libuv.a libuv.lib
    PATHS ${LIBUV_LIB_DIR}
)

find_library(LIBCRYPTO
    NAMES libcrypto.a libcrypto.lib libcrypto_static.lib
    PATHS ${OPENSSL_LIB_DIR}
)

if (ENABLE_GUI)
find_library(LIBFLTK
    NAMES libfltk.a libfltk.lib
    PATHS ${FLTK_LIB_DIR}
)
    message(STATUS "Find libfltk: ${LIBFLTK}")
    if (NOT LIBFLTK)
        message(FATAL_ERROR "FLTK lib not found!")
    endif()
endif()

message(STATUS "Find libuv: ${LIBUV}")
message(STATUS "Find libcrypto: ${LIBCRYPTO}")

if (NOT ( LIBUV AND LIBCRYPTO ) )
    message(FATAL_ERROR "required lib not found!")
endif()


# include_directories(/usr/local/include)

if (WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif (DARWIN)
    add_compile_definitions(PLATFORM_MACOS)
endif()

add_compile_definitions(APP_VERSION="${PROJECT_VERSION}")
add_compile_options("-Wall")

if (ENABLE_DEBUG)
    message(STATUS "Debug compile enabled")
    add_compile_options("-g")
endif()

if (ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

set(SRC
    src/capture/capturer.cpp
    src/utils.cpp
    src/logger.cpp
    src/pages.cpp
    src/websocket.cpp
    src/server.cpp
)

if (ENABLE_GUI)
    list(APPEND SRC
        src/gui.cpp
        src/main_gui.cpp
    )
else()
    list(APPEND SRC
        src/main.cpp
    )
endif()

if (DARWIN AND ENABLE_GUI)
    set(APP_ICON ${CMAKE_SOURCE_DIR}/src/assets/favicon.ico)

    set_source_files_properties(${APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    configure_file(
        ${CMAKE_SOURCE_DIR}/Info.plist.in  # 输入：模板文件
        ${CMAKE_BINARY_DIR}/Info.plist     # 输出：最终文件（放入 App 包）
        @ONLY  # 仅替换 @变量@ 格式的占位符
    )

    add_executable(deskshare MACOSX_BUNDLE 
        ${SRC}
        ${CMAKE_BINARY_DIR}/Info.plist
        ${APP_ICON}
    )

    set_target_properties(deskshare PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME DeskShare
        MACOSX_BUNDLE_DISPLAY_NAME "DeskShare"
        # 指定 App 包的图标（必须与 Info.plist 中的图标名称一致）
        MACOSX_BUNDLE_ICON_FILE favicon.ico
        # 指定图标文件在 App 包中的路径（固定为 Resources）
        MACOSX_BUNDLE_RESOURCES ${APP_ICON}
        # 可选：设置 App 版本、签名等
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.deskshare"  # 唯一标识（反向域名格式）
        # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist" 
    )

else()
    add_executable(deskshare ${SRC})

endif()

set(LIBS
    ${LIBUV}
    ${LIBCRYPTO}
)

if (ENABLE_GUI)
    list(APPEND LIBS ${LIBFLTK})
endif()

if (DARWIN)
    list(APPEND LIBS
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
        "-framework CoreServices"
        "-framework Cocoa"
    )

elseif (WIN32)
    if (ENABLE_GUI)
        list(APPEND LIBS "-mwindows")
    endif()

    list(APPEND LIBS
        ws2_32
        userenv
        dbghelp
        iphlpapi
        user32 
        advapi32
        psapi
        gdi32
        gdiplus
    )
endif()

target_link_libraries(deskshare PRIVATE ${LIBS})


if(WIN32 AND ENABLE_GUI)
    # 图标文件路径
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/favicon.ico")
    # 创建资源文件
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/icon.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/icon.rc
        @ONLY
    )
    # 编译资源文件并链接
    if(MINGW)
        # MinGW 使用 windres
        add_custom_command(
            OUTPUT icon.o
            COMMAND windres ${CMAKE_CURRENT_BINARY_DIR}/icon.rc -o icon.o
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/icon.rc ${ICON_FILE}
        )
        target_sources(deskshare PRIVATE icon.o)
    else()
        # MSVC 直接添加资源文件
        target_sources(deskshare PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/icon.rc)
    endif()
    target_sources(deskshare PRIVATE version.rc)
endif()



if (ENABLE_TEST)
    add_library(unittest STATIC
        src/capture/capturer.h
        src/capture/capturer.cpp
        src/utils.h
        src/utils.cpp
    )

    target_link_libraries(unittest PRIVATE
        ${LIBS}
    )

    add_subdirectory(test)
endif()
