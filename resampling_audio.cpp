/**
  * @example resampling_audio.c
  * libswresample API use example.
  */


#include "global.h"
static int get_format_from_sample_fmt(const char** fmt,
        enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char* fmt_be, * fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry* entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    fprintf(stderr,
        "Sample format %s not supported as output format\n",
        av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

int main(int argc, char** argv)
{
    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_STEREO;
    int src_rate = 44100, dst_rate = 44100;
    uint8_t** src_data = NULL, ** dst_data = NULL;
    int src_nb_channels = 0, dst_nb_channels = 0;
    int src_linesize, dst_linesize;
    int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_FLT, dst_sample_fmt = AV_SAMPLE_FMT_S16;
    const char* src_filename = NULL;
    const char* des_filename = NULL;
    FILE* src_file = nullptr;
    FILE* dst_file = nullptr;
    const char* fmt;
    struct SwrContext* swr_ctx;
    double t;
    int ret;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s output_file\n"
            "API example program to show how to resample an audio stream with libswresample.\n"
            "This program generates a series of audio frames, resamples them to a specified "
            "output format and rate and saves them to an output file named output_file.\n",
            argv[0]);
        exit(1);
    }
    src_filename = argv[1];
    src_file = fopen(src_filename, "rb");
    uint8_t src[4096];
    memset(src,0,4096);
    ret = fread(src,4096,1,src_file);
    if (!src_file) {
        fprintf(stderr, "Could not open src file %s\n", src_filename);
        exit(1);
    }

    des_filename = argv[2];
    dst_file = fopen(des_filename, "ab");
    if (!dst_file) {
        fprintf(stderr, "Could not open des file %s\n", des_filename);
        exit(1);
    }

    //1. 重采样上下文
    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout", src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);
    av_opt_set_int(swr_ctx, "out_channel_layout", dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        goto end;
    }

    //2. 输入缓冲区
    /* allocate source and destination samples buffers */
    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
        src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto end;
    }


    //3. 输出缓冲区
    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    //a*b/c  计算重采样输出样本数
    max_dst_nb_samples = dst_nb_samples =
        av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
        dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        goto end;
    }
    //重采样buffer大小
    int dst_bufsize = dst_nb_channels * dst_nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

    //4. 重采样
    while(fread(src_data[0],src_linesize,1,src_file)>0){
        int swrConvertRet = swr_convert(swr_ctx,
                                    dst_data,
                                    dst_nb_samples,
                                    (const uint8_t **) src_data,
                                    src_nb_samples);

        fwrite(dst_data[0], dst_bufsize, 1, dst_file);
    }

end:
    fclose(dst_file);
    fclose(src_file);
    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);
    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);
    swr_free(&swr_ctx);
    return ret < 0;
}
