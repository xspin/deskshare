#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR     // ERROR conflicts with Windows headers
};

#define LOG_STIME   0x1
#define LOG_TIME    0x2
#define LOG_LEVEL   0x4
#define LOG_LINE    0x8
#define LOG_FUNC    0xa
#define LOG_ALL (LOG_TIME|LOG_LEVEL|LOG_LINE|LOG_FUNC)

class Logger {
private:
    std::ostream* outputStream;
    LogLevel currentLevel;
    bool useColor;
    std::mutex logMutex;
    std::ofstream fileStream;
    bool logToFile;
    unsigned int mask;
    
    std::string getLevelString(LogLevel level);
    std::string getColoredLevelString(LogLevel level);
    
public:
    Logger(LogLevel level = LogLevel::INFO, bool color = true);
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void setOutStream(std::ostream* os);
    LogLevel getLogLevel();
    void setLogLevel(LogLevel level);
    void setColor(bool color);
    void setLogFile(const std::string& filename);
    void setMask(unsigned int mask);
    
    void log(LogLevel level, const std::string& function, int line,
        const std::string& file, const char *format, ...); 
};

// 全局日志实例
extern Logger gLogger;

class LogStream {
private:
    std::stringstream os;
    LogLevel level;
    std::string function;
    int line;
    std::string file;

public:
    LogStream(LogLevel level, const std::string& function, int line, const std::string& file):
        level(level), function(function), line(line), file(file)
    {}
    
    ~LogStream() {
        gLogger.log(level, function, line, file, os.str().c_str());
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        if (level >= gLogger.getLogLevel()) {
            os << value;
        }
        return *this;
    }
};

static inline Logger& getLogger() {
    return gLogger;
}

// 基础日志宏
#define LOG_DEBUG(...) \
    gLogger.log(LogLevel::DEBUG, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_INFO(...) \
    gLogger.log(LogLevel::INFO, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_WARNING(...) \
    gLogger.log(LogLevel::WARNING, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)

#define LOG_ERROR(...) \
    gLogger.log(LogLevel::ERR, __FUNCTION__, __LINE__, __FILE__, __VA_ARGS__)


// 流式输出日志宏
#define LOG_STREAM(level) \
    LogStream(level, __FUNCTION__, __LINE__, __FILE__)

#define LOG_DEBUG_STREAM        LOG_STREAM(LogLevel::DEBUG)
#define LOG_INFO_STREAM         LOG_STREAM(LogLevel::INFO)
#define LOG_WARNING_STREAM      LOG_STREAM(LogLevel::WARNING)
#define LOG_ERROR_STREAM        LOG_STREAM(LogLevel::ERR)

#endif // LOGGER_H