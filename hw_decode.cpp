#include "global.h"


/*
作用：用于硬件解码
函数入参：argc == 3,  
argv[0] =  hw_decode.exe (默认值，为程序路径)
argv[1] =  硬件加速组件,如"cuda"  "qsv"
argv[2] =  输入文件 
*/

AVPixelFormat g_pixformat;
static FILE* output_file = NULL;

enum AVPixelFormat get_hw_format(struct AVCodecContext* s, const enum AVPixelFormat* fmt)
{
	const enum AVPixelFormat* p;
	bool find_pixformat = false;
	for (p = fmt; *p != -1; p++) {
		AVPixelFormat avfmt = *p;
		std::cout << "get_hw_format name:" << av_get_pix_fmt_name(avfmt) << std::endl;
		/// _hw_pix_fmt是你的当前硬件设备支持的并且是你想使用的格式
		if (avfmt == AV_PIX_FMT_YUV420P) {
		//	return *p;
		}
		else if (avfmt == g_pixformat)
		{
			find_pixformat = true;
			//break;
		}

	}
	if (find_pixformat)
	{
		return g_pixformat;
	}
	else
	{
		std::cout << "fail to get hw format" << std::endl;
		return AV_PIX_FMT_NONE;
	}

}



static int decode_write(AVCodecContext* avctx, AVPacket* packet) {
	AVFrame* frame = nullptr;
	AVFrame* sw_frame = nullptr;
	AVFrame* tmp_frame = nullptr;
	int ret = avcodec_send_packet(avctx, packet);
	/*  avcodec_send_packet的返回值
	0：send_packet返回值为0，正常状态，意味着输入的packet被解码器正常接收
	EAGAIN：send_packet返回值为EAGAIN，输入的packet未被接收，需要输出一个或多个的frame后才能重新输入当前packet。
	EOF：send_packet返回值为EOF，当send_packet输入为NULL时才会触发该状态，用于通知解码器输入packet已结束。
	*/
	if (ret < 0)
	{
		return ret;
	}

	while (1)
	{
		if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc()))
		{
			std::cout << "alloc frame error" << std::endl;
			break;
		}
		/*	avcodec_receive_frame返回值
		receive 0            ：receive_frame返回值为0，正常状态，意味着已经输出一帧。
		receive EAGAIN：receive_frame返回值为EAGAIN，未能输出frame，需要输入更多的packet才能输出当前frame。
		receive EOF       ：receive_frame返回值为EOF，当处于send EOF状态后，调用一次或者多次receive_frame后就能得到该状态，表示所有的帧已经被输出。
		*/
		ret = avcodec_receive_frame(avctx, frame);
		//receive_frame返回值为EAGAIN，未能输出frame，需要输入更多的packet才能输出当前frame。
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			return 0;
		}
		else if (ret < 0)
		{
			std::cout << "decode error" << std::endl;
			goto fail;
		}
		if (frame->format == g_pixformat)
		{
			// 如果采用的硬件加速，则调用avcodec_receive_frame()函数后，解码后的数据还在GPU中，所以需要通过av_hwframe_transfer_data函数将GPU中的数据转移到CPU中来
			/* retrieve data from GPU to CPU */
			if (ret = av_hwframe_transfer_data(sw_frame, frame, 0) < 0)
			{
				goto fail;
			}
			tmp_frame = sw_frame;
		}
		else
		{
			tmp_frame = frame;
		}

		int size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
		void* buffer = av_malloc(size);
		ret = av_image_copy_to_buffer((uint8_t*)buffer, size, (const uint8_t* const*)tmp_frame->data, (const int*)tmp_frame->linesize, (AVPixelFormat)tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
		if (ret < 0)
		{
			goto fail;
		}
		if ((ret = fwrite(buffer, 1, size, output_file)) < 0) 
		{           
			goto fail; 
		}
	fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
		av_freep(&buffer);
	}
}
int main(int argc, char* argv[])
{

	AVFormatContext* avformat_ctx = nullptr;
	if (argc < 3)
	{
		return -1;
	}
	//第一步：查找硬件加速组件类型
	const char* hw_name = argv[1];
	AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(hw_name);
	if (hw_type == AV_HWDEVICE_TYPE_NONE)
	{
		std::cout << "can't find hardware device by name:" << hw_name << std::endl;
		return -1;
	}
	//第二步：打开输入文件
	const char* file_name = argv[2];
	int ret = avformat_open_input(&avformat_ctx, file_name, nullptr, nullptr);
	if (ret<0)
	{
		std::cout << "avformat_open_input error, return:" << ret << std::endl;
		return -2;
	}
	
	if (avformat_find_stream_info(avformat_ctx, nullptr))
	{
		std::cout<< "Can not find stream info" << std::endl;
		return -3;
	}
	
	//第三步：文件中查找视频流
	AVCodec* decoder = nullptr;
	int video_stream_index = av_find_best_stream(avformat_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (video_stream_index < 0)
	{
		std::cout << "can't find video stream" << std::endl;
		return -4;
	}

	//第四步：判断当前运行环境下的AVCodec是否支持所选择的硬件加速组件， 获取硬件加速组件支持的像素格式
	for (int i = 0; ; i++)
	{
		const AVCodecHWConfig* hw_conf = avcodec_get_hw_config(decoder, i);
		if (!hw_conf)
		{
			std::cout << "Decoder [" << decoder->name << "] does not support device type:" << av_hwdevice_get_type_name(hw_type) << std::endl;
			return -5;
		}
		if (hw_conf->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && hw_conf->device_type == hw_type)
		{
			g_pixformat = hw_conf->pix_fmt;
			break;
		}
	}

	//第五步：分配解码器上下文，初始化像素格式
	AVCodecContext* decodec_ctx = avcodec_alloc_context3(decoder);
	if (!decodec_ctx)
	{
		return -6;
	}

	AVStream* video_stram = avformat_ctx->streams[video_stream_index];
	//视频流信息拷贝到解码器上下文
	if (avcodec_parameters_to_context(decodec_ctx, video_stram->codecpar) < 0)
	{
		return -7;
	}
	//告诉解码器，需要解码的像素格式（协商）
	decodec_ctx->get_format = get_hw_format;

	//第六步  打开硬件加速组件，并得到一个硬件加速上下文AVHWDeviceContext
	AVBufferRef* hw_device_ctx = nullptr;
	int err = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0);
	if (err < 0)
	{
		return -8;
	}

	decodec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	//第七步：打开解码器
	ret = avcodec_open2(decodec_ctx, decoder, NULL);
	if (ret < 0)
	{
		return -9;
	}
	/* open the file to dump raw data */  
	const char* output_file_name = argv[3];
	output_file = fopen(output_file_name, "wb+");

	AVPacket packet;
	while (ret >= 0)
	{
		if (ret = av_read_frame(avformat_ctx, &packet) < 0)
			break;

		if (video_stream_index == packet.stream_index)
		{
			ret = decode_write(decodec_ctx, &packet);
		}
		av_packet_unref(&packet);
	}
	//flush decoder
	packet.data = NULL;
	packet.size = 0;
	decode_write(decodec_ctx, &packet);
	av_packet_unref(&packet);
	if (output_file)
	{
		fclose(output_file);
	}
	avcodec_free_context(&decodec_ctx);
	avformat_close_input(&avformat_ctx);
	av_buffer_unref(&hw_device_ctx);
	return 0;
}