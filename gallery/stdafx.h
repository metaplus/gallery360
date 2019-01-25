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
#pragma warning(push)
#pragma warning(disable:4267)
#include "core/pch.h"
#include "multimedia/pch.h"
#pragma warning(pop)

#include <d3d11.h>
#include <leveldb/db.h>
#include <boost/circular_buffer.hpp>
#include <range/v3/view/iota.hpp>
#include <blockingconcurrentqueue.h>
#include <readerwriterqueue.h>
#include <fstream>

#include "gallery/pch.h"
#include "unity/IUnityGraphicsD3D11.h"

#pragma comment(lib,"Ws2_32")
#pragma comment(lib,"Shlwapi")