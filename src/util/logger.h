#ifndef CC_UTIL_LOGGER_H
#define CC_UTIL_LOGGER_H

#include <string>
#include <format>
#include <iomanip>
#include <iostream>

namespace cc::util {
    // limited logging functionality
    // (should be updated to source_location and use log sinks)

    enum class LogLevel : int {
        DEBUG   = 0,
        INFO    = 1,
        WARNING = 2,
        ERR     = 3
    };

    class Logger {
    public:
        static Logger& instance() { static Logger x; return x; }

        void     set_level(LogLevel level) { m_level = level; }
        LogLevel get_level() const         { return m_level; }

        template <typename... Args>
        void log(LogLevel level, std::format_string<Args...> format, Args&&... args) {
            if (level >= m_level) {
                auto message = std::format(format, std::forward<Args>(args)...);
                std::cout << "[" << level_to_string(level) << "] " << message << std::endl;
            }
        }

        template <typename... Args>
        void debug(std::format_string<Args...> format, Args&&... args) {
            log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void info(std::format_string<Args...> format, Args&&... args) {
            log(LogLevel::INFO, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void warning(std::format_string<Args...> format, Args&&... args) {
            log(LogLevel::WARNING, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void error(std::format_string<Args...> format, Args&&... args) {
            log(LogLevel::ERR, format, std::forward<Args>(args)...);
        }

    private:
        Logger() = default;

        LogLevel m_level = LogLevel::INFO;

        static const char* level_to_string(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG:   return "DEBUG";
                case LogLevel::INFO:    return "INFO";
                case LogLevel::WARNING: return "WARN";
                case LogLevel::ERR:     return "ERROR";
                default:
                                        return "UNKNOWN";
            }
        }
    };
}

// Convenience macros
#define LOG_DEBUG(...)   cc::util::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)    cc::util::Logger::instance().info(__VA_ARGS__)
#define LOG_WARNING(...) cc::util::Logger::instance().warning(__VA_ARGS__)
#define LOG_ERROR(...)   cc::util::Logger::instance().error(__VA_ARGS__)

#endif
