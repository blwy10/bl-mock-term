// mtcore_stringutil.h                                                -*-C++-*-
#ifndef INCLUDED_MTCORE_STRINGUTIL
#define INCLUDED_MTCORE_STRINGUTIL

//@PURPOSE: Provide string manipulation helpers.

#include "mtcore_platform.h"

#include <string>
#include <vector>

namespace mtcore {

struct StringUtil {
    static std::string toUpper(const std::string& value);
    static std::string toLower(const std::string& value);
    static std::string trim(const std::string& value);

    static void split(std::vector<std::string> *result,
                      const std::string&        value,
                      char                      delimiter);
        // Append to 'result' the fields of 'value' separated by
        // 'delimiter'.  Empty fields are skipped.

    static std::string join(const std::vector<std::string>& values,
                            const char                     *separator);

    static bool startsWith(const std::string& value,
                           const std::string& prefix);
    static bool equalsNoCase(const std::string& lhs, const std::string& rhs);
    static bool containsNoCase(const std::string& haystack,
                               const std::string& needle);

    static std::string format(const char *format, ...);
        // printf-style formatting into a 'std::string'.

    static std::string fromInt64(Int64 value);
    static std::string fromDouble(double value, int decimals);
    static int toInt64(Int64 *result, const char *text);
        // Return 0 on success, non-zero if 'text' is not an integer.
    static int toDouble(double *result, const char *text);

    static Uint32 hash(const std::string& value);
        // FNV-1a; stable across processes and runs.

    static std::string firstWord(const std::string& value);
        // Text up to (not including) the first space.
};

}  // close namespace mtcore

#endif
