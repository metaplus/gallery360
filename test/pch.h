//
// pch.h
// Header for standard system include files.
//

#pragma once

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include <SDKDDKVer.h>

#pragma warning(disable: 4172)

#include "core/pch.h"
#include <folly/logging/xlog.h>
#include <folly/stop_watch.h>
#include <fstream>

#define GTEST_LANG_CXX11 1
#include "gtest/gtest.h"

#pragma comment(lib,"Ws2_32")
#pragma comment(lib,"Shlwapi")
#pragma comment(lib,"secur32")
#pragma comment(lib,"crypt32")
#pragma comment(lib,"bcrypt")

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;