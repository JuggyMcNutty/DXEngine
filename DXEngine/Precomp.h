#pragma once

#ifdef WIN32
#ifndef WINVER
#define WINVER 0x0A00 // Windows 10
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#undef PlaySound
#undef DrawText
#undef FindWindow
#undef MessageBox
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "Utils/Array.h"
#include "Utils/Exception.h"
#include "Utils/ScriptString.h"

// Every port is little endian, as the packages are: nothing to swap
#define BSWAP16(i) i
#define BSWAP32(i) i
#define BSWAP64(i) i

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__) || defined(__e2k__)) && defined(__SSE2__))
#define USE_SSE2
#else
#define NOSSE
#endif

#if defined(__ARM_NEON) || defined(_M_ARM64)
#define USE_NEON
#else
#define NONEON
#endif
