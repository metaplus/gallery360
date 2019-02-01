#pragma once

#define __STDC_CONSTANT_MACROS

#pragma warning(push)
#pragma warning(disable:4819)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#pragma warning(pop)

#include "multimedia/media.h"
#include "multimedia/io.cursor.h"
#include "multimedia/context.h"
#include "multimedia/io.segmentor.h"
