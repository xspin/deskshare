#include <iostream>
#include <cstdarg>
#include "logger.h"

Logger gLogger;

std::string Logger::getLevelString(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getColoredLevelString(LogLevel level) {
    if (!useColor) return getLevelString(level);
    
    switch(level) {
        case LogLevel::DEBUG: return "\033[36mDEBUG\033[0m";
        case LogLevel::INFO: return "\033[32mINFO\033[0m";
        case LogLevel::WARNING: return "\033[33mWARNING\033[0m";
        case LogLevel::ERR: return "\033[31mERROR\033[0m";
        default: return "\033[35mUNKNOWN\033[0m";
    }
}

Logger::Logger(LogLevel level, bool color)
    : currentLevel(level), useColor(color), logToFile(false), mask(LOG_ALL) {
    outputStream = &std::cerr;
}

Logger::~Logger() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

void Logger::setOutStream(std::ostream* os) {
    outputStream = os;
}

void Logger::setLogFile(const std::string& filename) {
    logToFile = true;
    fileStream.open(filename, std::ios::app);
    if (!fileStream.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        logToFile = false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::setMask(unsigned int mask) {
    this->mask = mask;
}

LogLevel Logger::getLogLevel() {
    return currentLevel;
}

void Logger::setColor(bool color) {
    useColor = color;
}

void Logger::log(LogLevel level, const std::string& function, int line, const std::string& file,
    const char *format, ...) {
    
    if (level < currentLevel) return;
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;

    if (mask & LOG_TIME) {
        ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    } else if (mask & LOG_STIME) {
        ss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    }
    if (mask & LOG_LEVEL) {
        ss << "[" << getColoredLevelString(level) << "]";
    }
    if (mask & LOG_LINE) {
        size_t i = file.rfind('/');
        if (i == std::string::npos) i = file.rfind('\\');
        ss << "[" << file.substr(i+1) << ":" << line << "]";
    }
    if (mask & LOG_FUNC) {
        ss << "[" << function << "]";
    }
    
    va_list args;
    va_start(args, format);
    char buffer[512];
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ss << " " << buffer << "\n";

    std::string logMessage = ss.str();
    
    // 输出到控制台
    *outputStream << logMessage;
    
    // 输出到文件（不带颜色）
    if (logToFile && fileStream.is_open()) {
        std::string fileMessage = ss.str();
        // 移除颜色代码
        size_t pos = 0;
        while ((pos = fileMessage.find("\033[", pos)) != std::string::npos) {
            size_t end = fileMessage.find("m", pos);
            if (end != std::string::npos) {
                fileMessage.erase(pos, end - pos + 1);
            }
        }
        fileStream << fileMessage << std::endl;
    }
}