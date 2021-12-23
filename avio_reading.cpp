#include "global.h"

struct buffer_data {
    uint8_t* ptr; /* 文件中对应位置指针 */
    size_t size; ///< size left in the buffer /* 文件当前指针到末尾 */   
};

/* 将文件中数据拷贝到缓冲区，同时文件指针位置偏移，数据大小改变 */
static int read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    struct buffer_data* bd = (struct buffer_data*)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

int main(int argc, char* argv[])
{
    AVFormatContext* fmt_ctx = NULL;
    AVIOContext* avio_ctx = NULL;
    uint8_t* buffer = NULL, * avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char* input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };

    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file\n"
            "API example program to show how to read from a custom buffer "
            "accessed through AVIOContext.\n", argv[0]);
        return 1;
    }
    input_filename = argv[1];

    /* register codecs and formats and other lavf/lavc components*/
    av_register_all();

    /* slurp file content into buffer */
    /* input_filename : 输入文件的文件名
     * buffer : 文件开始地址
     * buffer_size : 文件大小
     * 类似于UNIX下的mmap函数所实现的功能，返回文件开始指针，文件大小
     * 经测试，并不耗内存，可视为UNIX下的文件映射
    */
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0)
        goto end;

    /* fill opaque structure used by the AVIOContext read callback */
    /* bd 是指经过文件映射之后的文件，并不是指需要缓存区 */
    bd.ptr = buffer;
    bd.size = buffer_size;

    /* 初始化文件格式的结构体,就是分配内存 */
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* 分配内存, 可以自己设置缓冲大小,这里设置的是4K */
    avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /*          内存IO模式
     * avio_ctx_buffer是缓冲区，读写数据时用到。 avio_ctx_buffer的初始地址赋值到 AVIOContext::buffer
     * avio_ctx_buffer_size是缓冲区大小 , 也是每次读取数据的大小
     * bd    是输入文件文件的映射结构
     * read_packet 回调函数,读取数据的功能 , 具体在什么情况下才会回调 ?  --->avformat_open_input时会循环调用read_packet读取文件到缓冲区（每次最大4096Byte）
     */
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
        0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;
    /* 配置初始化信息
    * read_packet 回调函数会在这里被调用, 它将输入文件的所有数据都先存入缓存中,
    * 如果后面有需要用到数据，那么它就从缓存中直接调用数据
    */
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }

    /* 输出基本信息 */
    av_dump_format(fmt_ctx, 0, input_filename, 0);

end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    //应该就是av_free(&avio_ctx_buffer),但位置不对  
    //两者有差异  
    //printf("%p %p\n",avio_ctx_buffer,avio_ctx->buffer);  
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    av_file_unmap(buffer, buffer_size);

    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str2(ret));
        return 1;
    }

    return 0;
}