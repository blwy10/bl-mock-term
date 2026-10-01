// mtcore_log.h                                                       -*-C++-*-
#ifndef INCLUDED_MTCORE_LOG
#define INCLUDED_MTCORE_LOG

//@PURPOSE: Provide a process-wide printf-style logger and logging macros.
//
//@MACROS:
//  MT_LOG_TRACE, MT_LOG_DEBUG, MT_LOG_INFO, MT_LOG_WARN, MT_LOG_ERROR
//
//@USAGE:
//..
//  MT_LOG_INFO("session %d logged on as %s", id, user.c_str());
//..

namespace mtcore {

struct LogLevel {
    enum Value {
        e_TRACE = 0,
        e_DEBUG = 1,
        e_INFO  = 2,
        e_WARN  = 3,
        e_ERROR = 4,
        e_OFF   = 5
    };

    static const char *toString(Value value);
    static int fromString(const char *text, Value *result);
        // Return 0 and load 'result' on success, non-zero otherwise.
};

class Logger {
  public:
    static void setLevel(LogLevel::Value level);
    static LogLevel::Value level();
    static bool isEnabled(LogLevel::Value level);

    static int setFile(const char *path);
        // Additionally append log lines to 'path'.  Return 0 on success.
    static void setConsole(bool enabled);
    static void setProcessTag(const char *tag);
        // Prefix for every line, e.g. "mtserver".

    static void log(LogLevel::Value  level,
                    const char      *file,
                    int              line,
                    const char      *format,
                    ...);
    static void shutdown();
};

}  // close namespace mtcore

#define MT_LOG_AT(LEVEL, ...)                                                 \
    do {                                                                      \
        if (mtcore::Logger::isEnabled(LEVEL)) {                               \
            mtcore::Logger::log(LEVEL, __FILE__, __LINE__, __VA_ARGS__);      \
        }                                                                     \
    } while (0)

#define MT_LOG_TRACE(...) MT_LOG_AT(mtcore::LogLevel::e_TRACE, __VA_ARGS__)
#define MT_LOG_DEBUG(...) MT_LOG_AT(mtcore::LogLevel::e_DEBUG, __VA_ARGS__)
#define MT_LOG_INFO(...)  MT_LOG_AT(mtcore::LogLevel::e_INFO,  __VA_ARGS__)
#define MT_LOG_WARN(...)  MT_LOG_AT(mtcore::LogLevel::e_WARN,  __VA_ARGS__)
#define MT_LOG_ERROR(...) MT_LOG_AT(mtcore::LogLevel::e_ERROR, __VA_ARGS__)

#endif
