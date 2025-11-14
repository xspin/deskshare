cmake_minimum_required(VERSION 3.16)

project(deskshare VERSION 1.0)
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

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CAPTURE_DIR "${PROJECT_SOURCE_DIR}/src/capture")


if (WIN32)
    set(LIBUV_ROOT_DIR "C:/libuv-1.51.0")
    set(LIBUV_LIB_DIR "${LIBUV_ROOT_DIR}/build")

    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/x64/MT")
elseif (DARWIN)
    set(LIBUV_ROOT_DIR "/usr/local/opt/libuv")
    set(LIBUV_LIB_DIR "${LIBUV_ROOT_DIR}/lib")

    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib")
endif()

find_library(LIBUV
    NAMES libuv.a libuv.lib
    PATHS ${LIBUV_LIB_DIR})

find_library(LIBCRYPTO
    NAMES libcrypto.a libcrypto.lib libcrypto_static.lib
    PATHS ${OPENSSL_LIB_DIR}
)

if (NOT LIBUV)
    message(FATAL_ERROR "libuv not found!")
else()
    message("Find libuv: ${LIBUV}")
endif()

if (NOT LIBCRYPTO)
    message(FATAL_ERROR "libcrypto not found!")
else()
    message("Find libcrypto: ${LIBCRYPTO}")
endif()

include_directories(${LIBUV_ROOT_DIR}/include)
include_directories(${OPENSSL_ROOT_DIR}/include)
# include_directories(/usr/local/include)

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

if (DARWIN)
    add_library(capture ${CAPTURE_DIR}/capturer_macos.cpp)
elseif (WIN32)
    add_library(capture ${CAPTURE_DIR}/capturer_win.cpp)
else()
    message(FATAL_ERROR "Unsupported platform for capture library")
endif()

add_executable(deskshare src/main.cpp
    src/utils.cpp
    src/logger.cpp
    src/pages.cpp
    src/websocket.cpp
    src/server.cpp
)

if (WIN32)
  target_link_options(deskshare PRIVATE -static)
endif()

target_link_libraries(deskshare PRIVATE
    capture
    ${LIBUV}
    ${LIBCRYPTO}
)

if (DARWIN)
    target_link_libraries(deskshare PRIVATE
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
        "-framework CoreServices"
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
