#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
  // Windows 系统
  #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
  // macOS 系统
  #define PLATFORM_MACOS 1
#else
  // 其他系统（如 Linux）
  #define PLATFORM_UNKNOWN 1
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#endif