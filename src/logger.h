#pragma once

#include "singleton.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR // ERROR conflicts with Windows headers
};

#define LOG_STIME 0x1
#define LOG_TIME 0x2
#define LOG_LEVEL 0x4
#define LOG_LINE 0x8
#define LOG_FUNC 0xa
#define LOG_ALL (LOG_TIME | LOG_LEVEL | LOG_LINE | LOG_FUNC)

class Logger : public Singleton<Logger> {
    friend Singleton<Logger>;

  private:
    LogLevel currentLevel = LogLevel::INFO;
    bool useColor = true;
    std::mutex logMutex;
    std::ofstream fileStream;
    bool logToFile;
    unsigned int mask;
    std::ostream *outputStream = nullptr;

    std::string getLevelString(LogLevel level);
    std::string getColoredLevelString(LogLevel level);

    Logger();
    ~Logger();

  public:
    // static Logger &instance();

    void setOutStream(std::ostream *os);
    LogLevel getLogLevel();
    void setLogLevel(LogLevel level);
    void setColor(bool color);
    void setLogFile(const std::string &filename);
    void setMask(unsigned int mask);

    void log(LogLevel level, const std::string &function, int line, const std::string &file,
             const char *format, ...);
};

class LogStream {
  private:
    std::stringstream os;
    LogLevel level;
    std::string function;
    int line;
    std::string file;
    Logger &logger = Logger::instance();

  public:
    LogStream(LogLevel level, const std::string &function, int line, const std::string &file)
        : level(level), function(function), line(line), file(file) {
    }

    ~LogStream() {
        logger.log(level, function, line, file, os.str().c_str());
    }

    template <typename T> LogStream &operator<<(const T &value) {
        if (level >= logger.getLogLevel()) {
            os << value;
        }
        return *this;
    }
};

static inline Logger &getLogger() {
    return Logger::instance();
}

// 基础日志宏
#define LOG_DEBUG(...)                                                                             \
    Logger::instance().log(LogLevel::DEBUG, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_INFO(...)                                                                              \
    Logger::instance().log(LogLevel::INFO, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_WARNING(...)                                                                           \
    Logger::instance().log(LogLevel::WARNING, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_ERROR(...)                                                                             \
    Logger::instance().log(LogLevel::ERR, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

// 流式输出日志宏
#define LOG_STREAM(level) LogStream(level, __FUNCTION__, __LINE__, __FILE__)

#define LOG_DEBUG_STREAM LOG_STREAM(LogLevel::DEBUG)
#define LOG_INFO_STREAM LOG_STREAM(LogLevel::INFO)
#define LOG_WARNING_STREAM LOG_STREAM(LogLevel::WARNING)
#define LOG_ERROR_STREAM LOG_STREAM(LogLevel::ERR)
