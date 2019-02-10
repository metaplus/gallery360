// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#define _WIN32_WINNT 0x0A00             // Windows 10  
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
// Windows Header Files:
#include <windows.h>



// TODO: reference additional headers your program requires here
#include "core/config.h"
#include "gallery/pch.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"shlwapi")
#pragma comment(lib,"secur32")
#pragma comment(lib,"crypt32")
#pragma comment(lib,"bcrypt")
