#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace darkfire::net {

    enum class LogLevel : uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        None
    };

    inline const char* LogLevelToString(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Trace:   return "TRACE";
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }

    using LogCallback = std::function<void(LogLevel level, std::string_view message)>;

    class Logger {
    public:
        static Logger& Instance() {
            static Logger instance;
            return instance;
        }

        void SetLogLevel(LogLevel level) noexcept {
            m_minLevel = level;
        }

        [[nodiscard]] LogLevel GetLogLevel() const noexcept {
            return m_minLevel;
        }

        void SetLogCallback(LogCallback callback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_callback = std::move(callback);
        }

        void Log(LogLevel level, std::string_view message) {
            if (level < m_minLevel || m_minLevel == LogLevel::None) {
                return;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_callback) {
                m_callback(level, message);
            } else {
                // Default thread-safe fallback console logger
                auto now = std::chrono::system_clock::now();
                auto time_t_val = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
                
                std::tm tm_buf{};
#ifdef _WIN32
                localtime_s(&tm_buf, &time_t_val);
#else
                localtime_r(&time_t_val, &tm_buf);
#endif
                char time_str[32];
                std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

                std::ostream& os = (level >= LogLevel::Warning) ? std::cerr : std::cout;
                os << "[" << time_str << "." << (ms.count() < 100 ? (ms.count() < 10 ? "00" : "0") : "") << ms.count()
                   << "] [" << LogLevelToString(level) << "] " << message << "\n";
            }
        }

        template <typename... Args>
        void LogFmt(LogLevel level, Args&&... args) {
            if (level < m_minLevel || m_minLevel == LogLevel::None) {
                return;
            }
            std::ostringstream ss;
            (ss << ... << std::forward<Args>(args));
            Log(level, ss.str());
        }

    private:
        Logger() : m_minLevel(LogLevel::Info), m_callback(nullptr) {}
        LogLevel m_minLevel;
        LogCallback m_callback;
        std::mutex m_mutex;
    };

    inline void SetLogLevel(LogLevel level) noexcept {
        Logger::Instance().SetLogLevel(level);
    }

    inline void SetLogCallback(LogCallback callback) {
        Logger::Instance().SetLogCallback(std::move(callback));
    }

    template <typename... Args>
    inline void LogTrace(Args&&... args) {
        Logger::Instance().LogFmt(LogLevel::Trace, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogDebug(Args&&... args) {
        Logger::Instance().LogFmt(LogLevel::Debug, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogInfo(Args&&... args) {
        Logger::Instance().LogFmt(LogLevel::Info, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogWarn(Args&&... args) {
        Logger::Instance().LogFmt(LogLevel::Warning, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogError(Args&&... args) {
        Logger::Instance().LogFmt(LogLevel::Error, std::forward<Args>(args)...);
    }

} // namespace darkfire::net
