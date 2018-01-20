#pragma once
#define __STDC_CONSTANT_MACROS
extern "C" {
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/hwcontext.h>
#include <libavutil/file.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/dxva2.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

}
#ifdef _WIN32
#pragma comment(lib,"avcodec")
#pragma comment(lib,"avformat")
#pragma comment(lib,"avutil")
#pragma comment(lib,"swscale")
#endif //_WIN32