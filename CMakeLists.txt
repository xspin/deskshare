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


option(ENABLE_ASAN "Enable Address Sanitizer for memory leak detection" OFF)
option(ENABLE_DEBUG "Enable Debug compile" OFF)
option(ENABLE_GUI "Enable GUI build" ON)

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
    message("Find libfltk: ${LIBFLTK}")
    if (NOT LIBFLTK)
        message(FATAL_ERROR "FLTK lib not found!")
    endif()
endif()

message("Find libuv: ${LIBUV}")
message("Find libcrypto: ${LIBCRYPTO}")

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

add_executable(deskshare ${SRC})

if (WIN32 AND ENABLE_GUI)
    target_link_options(deskshare PRIVATE -static
        "-mwindows"
    )
endif()

target_link_libraries(deskshare PRIVATE
    ${LIBUV}
    ${LIBCRYPTO}
)
if (ENABLE_GUI)
    target_link_libraries(deskshare PRIVATE
        ${LIBFLTK}
    )
endif()

if (DARWIN)
    target_link_libraries(deskshare PRIVATE
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
        "-framework CoreServices"
        "-framework Cocoa"
    )
elseif (WIN32)
    target_link_libraries(deskshare PRIVATE
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