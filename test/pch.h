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

#pragma warning(disable: 4172 4146 4455)

#include "core/config.h"
#include "core/core.h"
#include <folly/logging/xlog.h>
#include <folly/stop_watch.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fstream>

#define GTEST_LANG_CXX11 1
#include "gtest/gtest.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"shlwapi")
#pragma comment(lib,"secur32")
#pragma comment(lib,"crypt32")
#pragma comment(lib,"bcrypt")

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;
using std::string_literals::operator ""s;
using std::string_view_literals::operator ""sv;
using std::chrono_literals::operator ""ms;
using std::chrono_literals::operator ""s;
