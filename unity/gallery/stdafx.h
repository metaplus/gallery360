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
#include <d3d11.h>
#include "core/pch.h"
#include "multimedia/pch.h"
#include "unity/gallery/pch.h"
#include "unity/detail/PlatformBase.h"
#include "unity/detail/IUnityInterface.h"
#include "unity/detail/IUnityGraphics.h"
#include "unity/detail/IUnityGraphicsD3D11.h"
#include "unity/gallery/pch.h"
//#include "unity/gallery/interface.h"
#include "unity/gallery/interprocess.h"
#include "unity/gallery/graphic.h"

#pragma comment(lib, "core")
#pragma comment(lib, "multimedia")