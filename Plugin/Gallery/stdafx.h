// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>



// TODO: reference additional headers your program requires here
#include "Core/pch.h"
#include "FFmpeg/pch.h"
#include "FFmpeg/base.h"
#include "FFmpeg/context.h"
#include "Core/base.hpp"
#include "Core/verify.hpp"
#include <d3d11.h>
#include "Unity/IUnityGraphicsD3D11.h"
#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityInterface.h"
#include "Gallery/render.h"
//#include "Gallery/interface.h"
#include <boost/interprocess/ipc/message_queue.hpp>



#pragma comment(lib,"Core")
#pragma comment(lib,"FFmpeg")
