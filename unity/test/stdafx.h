// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include "targetver.h"

// Headers for CppUnitTest
#include "CppUnitTest.h"

// TODO: reference additional headers your program requires here
#pragma warning(disable: 4996)
#define CORE_USE_BOOST_ASIO
#include "core/pch.h"
#include "multimedia/pch.h"
#include "network/pch.h"
#include "gallery/pch.h"
#include "gallery/export.h"

#pragma comment(lib, "core")
#pragma comment(lib, "multimedia")
#pragma comment(lib, "gallery")