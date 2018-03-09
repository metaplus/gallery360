// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include <WinSock2.h>
#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include "Core/pch.h"
#include "FFmpeg/pch.h"
#include "Gallery/pch.h"
#include "Monitor/pch.h"
#include "Core/base.hpp"
#include "Core/verify.hpp"
#include "Core/basic_ptr.hpp"
#include "FFmpeg/base.h"
#include "FFmpeg/context.h"
//#include "Gallery/pch.h"
#include "Gallery/openvr.h"
#include "Gallery/interprocess.h"
#include "Gallery/interface.h"
#include "Monitor/base.hpp"
#pragma comment(lib,"Core")
#pragma comment(lib,"FFmpeg")
#pragma comment(lib,"Gallery")