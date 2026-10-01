// mtcore_platform.h                                                  -*-C++-*-
#ifndef INCLUDED_MTCORE_PLATFORM
#define INCLUDED_MTCORE_PLATFORM

//@PURPOSE: Provide platform includes and fixed-width integer typedefs.
//
//@DESCRIPTION: Every 'mtcore', 'mtsrv' and 'mtcomm' translation unit includes
// this header first so that <winsock2.h> is always seen before <windows.h>.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

namespace mtcore {

typedef signed char        Int8;
typedef unsigned char      Uint8;
typedef short              Int16;
typedef unsigned short     Uint16;
typedef int                Int32;
typedef unsigned int       Uint32;
typedef __int64            Int64;
typedef unsigned __int64   Uint64;

}  // close namespace mtcore

#define MTCORE_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif
