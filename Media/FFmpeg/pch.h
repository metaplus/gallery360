#pragma once
#define __STDC_CONSTANT_MACROS
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
//#pragma warning(default:4819)
#pragma warning(pop)
#ifdef _WIN32
#pragma comment(lib,"avcodec")
#pragma comment(lib,"avformat")
#pragma comment(lib,"avutil")
#pragma comment(lib,"swscale")
#endif //_WIN32