// mtcore_log.cpp                                                     -*-C++-*-
#include "mtcore_log.h"

#include "mtcore_mutex.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace mtcore {

namespace {

Mutex& logMutex()
{
    static Mutex s_mutex;
    return s_mutex;
}

volatile int    s_level   = LogLevel::e_INFO;
bool            s_console = true;
FILE           *s_file    = NULL;
char            s_tag[32] = "mt";

const char *baseName(const char *path)
{
    const char *slash = strrchr(path, (char)92);  // backslash
    const char *fwd   = strrchr(path, '/');
    if (fwd > slash) {
        slash = fwd;
    }
    return slash ? slash + 1 : path;
}

}  // close unnamed namespace

const char *LogLevel::toString(Value value)
{
    switch (value) {
      case e_TRACE: return "TRACE";
      case e_DEBUG: return "DEBUG";
      case e_INFO:  return "INFO";
      case e_WARN:  return "WARN";
      case e_ERROR: return "ERROR";
      case e_OFF:   return "OFF";
    }
    return "???";
}

int LogLevel::fromString(const char *text, Value *result)
{
    static const char *names[] = { "TRACE", "DEBUG", "INFO",
                                   "WARN",  "ERROR", "OFF" };
    for (int i = 0; i < (int)MTCORE_ARRAY_SIZE(names); ++i) {
        if (_stricmp(text, names[i]) == 0) {
            *result = (Value)i;
            return 0;
        }
    }
    return -1;
}

void Logger::setLevel(LogLevel::Value level)
{
    s_level = level;
}

LogLevel::Value Logger::level()
{
    return (LogLevel::Value)s_level;
}

bool Logger::isEnabled(LogLevel::Value level)
{
    return (int)level >= s_level;
}

int Logger::setFile(const char *path)
{
    LockGuard guard(&logMutex());
    if (s_file) {
        fclose(s_file);
        s_file = NULL;
    }
    if (path == NULL || *path == 0) {
        return 0;
    }
    s_file = fopen(path, "a");
    return s_file ? 0 : -1;
}

void Logger::setConsole(bool enabled)
{
    s_console = enabled;
}

void Logger::setProcessTag(const char *tag)
{
    LockGuard guard(&logMutex());
    strncpy(s_tag, tag, sizeof(s_tag) - 1);
    s_tag[sizeof(s_tag) - 1] = 0;
}

void Logger::log(LogLevel::Value  level,
                 const char      *file,
                 int              line,
                 const char      *format,
                 ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    _vsnprintf(message, sizeof(message) - 1, format, args);
    va_end(args);
    message[sizeof(message) - 1] = 0;

    SYSTEMTIME st;
    ::GetLocalTime(&st);

    char header[160];
    _snprintf(header,
              sizeof(header) - 1,
              "%02d:%02d:%02d.%03d %-5s %s[%lu] %s:%d ",
              st.wHour,
              st.wMinute,
              st.wSecond,
              st.wMilliseconds,
              LogLevel::toString(level),
              s_tag,
              ::GetCurrentThreadId(),
              baseName(file),
              line);
    header[sizeof(header) - 1] = 0;

    LockGuard guard(&logMutex());
    if (s_console) {
        fprintf(stderr, "%s%s\n", header, message);
        fflush(stderr);
    }
    if (s_file) {
        fprintf(s_file, "%s%s\n", header, message);
        fflush(s_file);
    }
}

void Logger::shutdown()
{
    setFile(NULL);
}

}  // close namespace mtcore
