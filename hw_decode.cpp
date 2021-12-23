#include "global.h"


/*
���ã�����Ӳ������
������Σ�argc == 3,  
argv[0] =  hw_decode.exe (Ĭ��ֵ��Ϊ����·��)
argv[1] =  Ӳ���������,��"cuda"  "qsv"
argv[2] =  �����ļ� 
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
		/// _hw_pix_fmt����ĵ�ǰӲ���豸֧�ֵĲ���������ʹ�õĸ�ʽ
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
	/*  avcodec_send_packet�ķ���ֵ
	0��send_packet����ֵΪ0������״̬����ζ�������packet����������������
	EAGAIN��send_packet����ֵΪEAGAIN�������packetδ�����գ���Ҫ���һ��������frame������������뵱ǰpacket��
	EOF��send_packet����ֵΪEOF����send_packet����ΪNULLʱ�Żᴥ����״̬������֪ͨ����������packet�ѽ�����
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
		/*	avcodec_receive_frame����ֵ
		receive 0            ��receive_frame����ֵΪ0������״̬����ζ���Ѿ����һ֡��
		receive EAGAIN��receive_frame����ֵΪEAGAIN��δ�����frame����Ҫ��������packet���������ǰframe��
		receive EOF       ��receive_frame����ֵΪEOF��������send EOF״̬�󣬵���һ�λ��߶��receive_frame����ܵõ���״̬����ʾ���е�֡�Ѿ��������
		*/
		ret = avcodec_receive_frame(avctx, frame);
		//receive_frame����ֵΪEAGAIN��δ�����frame����Ҫ��������packet���������ǰframe��
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
			// ������õ�Ӳ�����٣������avcodec_receive_frame()�����󣬽��������ݻ���GPU�У�������Ҫͨ��av_hwframe_transfer_data������GPU�е�����ת�Ƶ�CPU����
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
	//��һ��������Ӳ�������������
	const char* hw_name = argv[1];
	AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(hw_name);
	if (hw_type == AV_HWDEVICE_TYPE_NONE)
	{
		std::cout << "can't find hardware device by name:" << hw_name << std::endl;
		return -1;
	}
	//�ڶ������������ļ�
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
	
	//���������ļ��в�����Ƶ��
	AVCodec* decoder = nullptr;
	int video_stream_index = av_find_best_stream(avformat_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (video_stream_index < 0)
	{
		std::cout << "can't find video stream" << std::endl;
		return -4;
	}

	//���Ĳ����жϵ�ǰ���л����µ�AVCodec�Ƿ�֧����ѡ���Ӳ����������� ��ȡӲ���������֧�ֵ����ظ�ʽ
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

	//���岽����������������ģ���ʼ�����ظ�ʽ
	AVCodecContext* decodec_ctx = avcodec_alloc_context3(decoder);
	if (!decodec_ctx)
	{
		return -6;
	}

	AVStream* video_stram = avformat_ctx->streams[video_stream_index];
	//��Ƶ����Ϣ������������������
	if (avcodec_parameters_to_context(decodec_ctx, video_stram->codecpar) < 0)
	{
		return -7;
	}
	//���߽���������Ҫ��������ظ�ʽ��Э�̣�
	decodec_ctx->get_format = get_hw_format;

	//������  ��Ӳ��������������õ�һ��Ӳ������������AVHWDeviceContext
	AVBufferRef* hw_device_ctx = nullptr;
	int err = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0);
	if (err < 0)
	{
		return -8;
	}

	decodec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	//���߲����򿪽�����
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