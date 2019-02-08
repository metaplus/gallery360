// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include "core/config.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"shlwapi")
#endif

#include <fmt/ostream.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef _WIN32
#include "server/app.h"
#include "server/server.hpp"
#endif
