#ifndef GLOBAL_H
#define GLOBAL_H

extern "C"
{
#include <libavcodec/avcodec.h>  
#include <libavformat/avformat.h>  
#include <libavformat/avio.h>  
#include <libavutil/file.h>  
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_qsv.h>
#include <libavutil/mem.h>
#include <libavutil/buffer.h>
#include <libavutil/common.h>
#include <libavutil/error.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avassert.h>
#include <libavutil/avstring.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>

char str[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str2(errnum) \
    av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum)

#endif // !1
