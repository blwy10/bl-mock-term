// mtcore_stringutil.cpp                                              -*-C++-*-
#include "mtcore_stringutil.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mtcore {

std::string StringUtil::toUpper(const std::string& value)
{
    std::string result(value);
    for (std::string::size_type i = 0; i < result.size(); ++i) {
        result[i] = (char)toupper((unsigned char)result[i]);
    }
    return result;
}

std::string StringUtil::toLower(const std::string& value)
{
    std::string result(value);
    for (std::string::size_type i = 0; i < result.size(); ++i) {
        result[i] = (char)tolower((unsigned char)result[i]);
    }
    return result;
}

std::string StringUtil::trim(const std::string& value)
{
    std::string::size_type begin = 0;
    std::string::size_type end   = value.size();
    while (begin < end && isspace((unsigned char)value[begin])) {
        ++begin;
    }
    while (end > begin && isspace((unsigned char)value[end - 1])) {
        --end;
    }
    return value.substr(begin, end - begin);
}

void StringUtil::split(std::vector<std::string> *result,
                       const std::string&        value,
                       char                      delimiter)
{
    std::string::size_type start = 0;
    while (start <= value.size()) {
        std::string::size_type pos = value.find(delimiter, start);
        if (pos == std::string::npos) {
            pos = value.size();
        }
        if (pos > start) {
            result->push_back(value.substr(start, pos - start));
        }
        start = pos + 1;
    }
}

std::string StringUtil::join(const std::vector<std::string>& values,
                             const char                     *separator)
{
    std::string result;
    for (std::vector<std::string>::const_iterator it = values.begin();
         it != values.end();
         ++it) {
        if (it != values.begin()) {
            result += separator;
        }
        result += *it;
    }
    return result;
}

bool StringUtil::startsWith(const std::string& value,
                            const std::string& prefix)
{
    return value.size() >= prefix.size()
        && value.compare(0, prefix.size(), prefix) == 0;
}

bool StringUtil::equalsNoCase(const std::string& lhs, const std::string& rhs)
{
    return lhs.size() == rhs.size() && _stricmp(lhs.c_str(), rhs.c_str()) == 0;
}

bool StringUtil::containsNoCase(const std::string& haystack,
                                const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string StringUtil::format(const char *format, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, format);
    int n = _vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    if (n < 0) {
        n = sizeof(buffer) - 1;
    }
    buffer[n] = 0;
    return std::string(buffer, n);
}

std::string StringUtil::fromInt64(Int64 value)
{
    char buffer[32];
    _snprintf(buffer, sizeof(buffer), "%I64d", value);
    return std::string(buffer);
}

std::string StringUtil::fromDouble(double value, int decimals)
{
    char buffer[64];
    _snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return std::string(buffer);
}

int StringUtil::toInt64(Int64 *result, const char *text)
{
    if (text == NULL || *text == 0) {
        return -1;
    }
    char *end = NULL;
    errno = 0;
    Int64 value = _strtoi64(text, &end, 10);
    if (errno != 0 || *end != 0) {
        return -1;
    }
    *result = value;
    return 0;
}

int StringUtil::toDouble(double *result, const char *text)
{
    if (text == NULL || *text == 0) {
        return -1;
    }
    char *end = NULL;
    double value = strtod(text, &end);
    if (*end != 0) {
        return -1;
    }
    *result = value;
    return 0;
}

Uint32 StringUtil::hash(const std::string& value)
{
    Uint32 h = 2166136261U;
    for (std::string::size_type i = 0; i < value.size(); ++i) {
        h ^= (Uint8)value[i];
        h *= 16777619U;
    }
    return h;
}

std::string StringUtil::firstWord(const std::string& value)
{
    std::string::size_type sp = value.find(' ');
    return sp == std::string::npos ? value : value.substr(0, sp);
}

}  // close namespace mtcore
