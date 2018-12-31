// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include "targetver.h"




// TODO: reference additional headers your program requires here
#include "core/pch.h"
#include "multimedia/pch.h"
#include "multimedia/component.h"
#include <range/v3/view/iota.hpp>

#pragma warning(push)
#pragma warning(disable:4819)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/dxva2.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avassert.h>
#include <libavutil/error.h>
#include <libavutil/file.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}
#pragma warning(pop)

#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swscale")
