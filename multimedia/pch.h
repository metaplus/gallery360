#pragma once

#define __STDC_CONSTANT_MACROS

#pragma warning(push)
#pragma warning(disable:4819)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#pragma warning(pop)

#ifdef _WIN64
#pragma comment(lib,"avcodec")
#pragma comment(lib,"avformat")
#pragma comment(lib,"avutil")
#pragma comment(lib,"swscale")
#endif  // _WIN64

#include "multimedia/ffmpeg.h"
#include "multimedia/cursor.h"
#include "multimedia/context.h"
