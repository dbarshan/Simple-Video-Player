#include "Player.h"
#include <assert.h>

void audio_callback(void*, Uint8*, int);
int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size);
int getFrames(void *);

AVFrame wanted_frame;
Packet audioq;


Player::Player() {
	av_register_all();
	vidoePacket = NULL;
	pFormatCtx = NULL;
	videoCodecParm = NULL;
	audioCodecParm = NULL;
	videoStream = -1;
	audioStream = -1;
	videoCodec = NULL;
	audioCodec = NULL;
	videoCodecCtx = NULL;
	audioCodecCtx = NULL;
	sws_ctx = NULL;
	buffer = NULL;
	pFrame = NULL;
	pFrameRGB = NULL;
}

Player::~Player() {

}

int Player::openFile(std::string &filename) {

	//open video
	int res = avformat_open_input(&pFormatCtx, filename.c_str(), NULL, NULL);

	//check video opened
	if (res != 0) {
		//error_msg = getAVError(res);
		return -1;
	}

	//get video info
	res = avformat_find_stream_info(pFormatCtx, NULL);
	if (res < 0) {
		//error_msg = getAVError(res);
		return -1;
	}

	

	//get video stream
	videoStream = getCodecParameters();
	if (videoStream == -1) {
		//error_msg = (char *) "Error opening your video using AVCodecParameters, does not have codecpar_type type AVMEDIA_TYPE_VIDEO";
		return -1;
	}

	videoFPS = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);

	if (getCodec() < 0)
		return -1;
}

int Player::getCodecParameters() {

	int videoIndex = -1;
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; ++i) {
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoIndex < 0)
			videoIndex = i;
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
			audioStream = i;
	}
	if (-1 == videoIndex) {
		//error_msg = (char *) "Could not get video stream";
		return -1;
	}
	videoCodecParm = pFormatCtx->streams[videoIndex]->codecpar;
	if (audioStream != -1) 
		audioCodecParm = pFormatCtx->streams[audioStream]->codecpar;
	return videoIndex;
}

int Player::getCodec() {

	videoCodec = avcodec_find_decoder(videoCodecParm->codec_id);
	audioCodec = avcodec_find_decoder(audioCodecParm->codec_id);

	if (videoCodec == NULL) {
		//error_msg = (char *) " bad video codec ";
		return -1; // Codec not found
	}

	if (audioCodec == NULL) {
	//	error_msg = (char *)" bad audio codec ";
		return -1; // Codec not found
	}

	videoCodecCtx = avcodec_alloc_context3(videoCodec);
	if (videoCodecCtx == NULL) {
		//error_msg = (char *)"Bad video codec";
		return (-1);
	}

	audioCodecCtx = avcodec_alloc_context3(audioCodec);
	if (audioCodecCtx == NULL) {
		//error_msg = (char *)"Bad audio codec";
		return (-1);
	}

	int res = avcodec_parameters_to_context(videoCodecCtx, videoCodecParm);
	if (res < 0) {
		//error_msg = (char *)"Failed to get video codec";
		avformat_close_input(&pFormatCtx);
		avcodec_free_context(&videoCodecCtx);
		return(-1);
	}

	res = avcodec_parameters_to_context(audioCodecCtx, audioCodecParm);

	if (res < 0) {
		//error_msg = (char *)"Failed to get audio codec";
		avformat_close_input(&pFormatCtx);
		avcodec_free_context(&videoCodecCtx);
		avcodec_free_context(&audioCodecCtx);
		return(-1);
	}

	res = avcodec_open2(videoCodecCtx, videoCodec, NULL);
	if (res < 0) {
		//error_msg = (char *)"Failed to open video codec";
		return(-1);
	}
	res = avcodec_open2(audioCodecCtx, audioCodec, NULL);

	if (res < 0) {
		//error_msg = (char *)"Failed to open audio codec";
		return(-1);
	}
	return 1;
}

void Player::getInfo() {
	av_dump_format(pFormatCtx, 0, pFormatCtx->filename, 0);
}

int Player::allocateMemory() {

	struct SwrContext *swrCtx = NULL;
	swrCtx = swr_alloc();
	if (swrCtx == NULL) {
		error_msg =  "Failed to load audio";
		return (-1);
	}

	//audio context
	av_opt_set_channel_layout(swrCtx, "in_channel_layout", av_get_default_channel_layout(audioCodecCtx->channels), 0);
	av_opt_set_channel_layout(swrCtx, "out_channel_layout", av_get_default_channel_layout(audioCodecCtx->channels), 0);
	av_opt_set_int(swrCtx, "in_sample_rate", audioCodecCtx->sample_rate, 0);
	av_opt_set_int(swrCtx, "out_sample_rate", audioCodecCtx->sample_rate, 0);
	av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", audioCodecCtx->sample_fmt, 0);
	av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

	int res = swr_init(swrCtx);

	if (res != 0) {
		error_msg = "Failed to initialize audio";
		return -1;
	}
	SDL_AudioSpec wantedSpec,audioSpec;
	memset(&wantedSpec, 0, sizeof(wantedSpec));
	wantedSpec.channels = audioCodecCtx->channels;
	wantedSpec.freq = audioCodecCtx->sample_rate;
	wantedSpec.format = AUDIO_S16SYS;
	wantedSpec.silence = 0;
	wantedSpec.samples = SDL_AUDIO_BUFFER_SIZE;
	wantedSpec.userdata = audioCodecCtx;
	wantedSpec.callback = audio_callback;
	
	if (SDL_OpenAudio(&wantedSpec, &audioSpec) < 0) {
		error_msg = "Error opening audio";
		return -1;
	}
	wanted_frame.format = AV_SAMPLE_FMT_S16;
	wanted_frame.sample_rate = audioSpec.freq;
	wanted_frame.channel_layout = av_get_default_channel_layout(audioSpec.channels);
	wanted_frame.channels = audioSpec.channels;

	initAudioPacket(&audioq);
	SDL_PauseAudio(0);

	pFrame = av_frame_alloc();
	if (pFrame == NULL) {
		error_msg = (char *) "Could not allocate frame";
		return -1;
	}

	pFrameRGB = av_frame_alloc();
	if (NULL == pFrameRGB) {
		error_msg = "Could not allocate frame RGB";
		return -1;
	}


	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoCodecCtx->width, videoCodecCtx->height, 1);

	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	res = av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, videoCodecCtx->width, videoCodecCtx->height, 1);
	if (res < 0) {
		error_msg = getAVError(res);
		return res;
	}
	return 0;
}

void Player::initAudioPacket(Packet *pkt)
{
	pkt->last = NULL;
	pkt->first = NULL;
	pkt->mutex = SDL_CreateMutex();
	pkt->cond = SDL_CreateCond();
}

int Player::getAudioPacket(Packet *q, AVPacket* pkt, int block) {

	AVPacketList* pktl;
	int ret;

	SDL_LockMutex(q->mutex);

	while (1)
	{
		pktl = q->first;
		if (pktl)
		{
			q->first = pktl->next;
			if (!q->first)
				q->last = NULL;

			q->nb_packets--;
			q->size -= pktl->pkt.size;

			*pkt = pktl->pkt;
			av_free(pktl);
			ret = 1;
			break;
		}
		else if (!block)
		{
			ret = 0;
			break;
		}
		else
		{
			SDL_CondWait(q->cond, q->mutex);
		}
	}

	SDL_UnlockMutex(q->mutex);

	return ret;

}

std::string Player::getAVError(int err) {
	char errobuf[ERROR_SIZE];
	av_strerror(err, errobuf, ERROR_SIZE);
	return ((std::string)errobuf);
}

int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size) {

	static AVPacket pkt;
	static uint8_t* audio_pkt_data = NULL;
	static int audio_pkt_size = 0;
	static AVFrame frame;

	int len1;
	int data_size = 0;

	SwrContext* swr_ctx = NULL;

	while (1)
	{
		while (audio_pkt_size > 0)
		{
			int got_frame = 0;

			avcodec_send_packet(aCodecCtx, &pkt);
			avcodec_receive_frame(aCodecCtx, &frame);

			len1 = frame.pkt_size;
			if (len1 < 0)
			{
				audio_pkt_size = 0;
				break;
			}

			audio_pkt_data += len1;
			audio_pkt_size -= len1;
			data_size = 0;
			if (got_frame)
			{
				int linesize = 1;
				data_size = av_samples_get_buffer_size(&linesize, aCodecCtx->channels, frame.nb_samples, aCodecCtx->sample_fmt, 1);
				assert(data_size <= buf_size);
				memcpy(audio_buf, frame.data[0], data_size);
			}

			if (frame.channels > 0 && frame.channel_layout == 0)
				frame.channel_layout = av_get_default_channel_layout(frame.channels);
			else if (frame.channels == 0 && frame.channel_layout > 0)
				frame.channels = av_get_channel_layout_nb_channels(frame.channel_layout);

			if (swr_ctx)
			{
				swr_free(&swr_ctx);
				swr_ctx = NULL;
			}

			swr_ctx = swr_alloc_set_opts(NULL, wanted_frame.channel_layout, (AVSampleFormat)wanted_frame.format, wanted_frame.sample_rate,
				frame.channel_layout, (AVSampleFormat)frame.format, frame.sample_rate, 0, NULL);

			if (!swr_ctx || swr_init(swr_ctx) < 0)
			{
				//std::cout << "swr_init failed" << std::endl;
				break;
			}

			int dst_nb_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, frame.sample_rate) + frame.nb_samples,
				wanted_frame.sample_rate, wanted_frame.format, AV_ROUND_INF);
			int len2 = swr_convert(swr_ctx, &audio_buf, dst_nb_samples,
				(const uint8_t**)frame.data, frame.nb_samples);
			if (len2 < 0)
			{
				//std::cout << "swr_convert failed" << std::endl;
				break;
			}

			return wanted_frame.channels * len2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

			if (data_size <= 0)
				continue;

			return data_size;
		}

		if (pkt.data)
			av_packet_unref(&pkt);

		if (Player::getAudioPacket(&audioq, &pkt, 1) < 0)
			return -1;

		audio_pkt_data = pkt.data;
		audio_pkt_size = pkt.size;
	}

}


void audio_callback(void* userdata, Uint8* stream, int len) {

	AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
	int len1, audio_size;

	static uint8_t audio_buff[192000 * 3 / 2];
	static unsigned int audio_buf_size = 0;
	static unsigned int audio_buf_index = 0;

	SDL_memset(stream, 0, len);

	while (len > 0)
	{
		if (audio_buf_index >= audio_buf_size)
		{
			audio_size = audio_decode_frame(aCodecCtx, audio_buff, sizeof(audio_buff));
			if (audio_size < 0)
			{
				audio_buf_size = 1024;
				memset(audio_buff, 0, audio_buf_size);
			}
			else
				audio_buf_size = audio_size;

			audio_buf_index = 0;
		}
		len1 = audio_buf_size - audio_buf_index;
		if (len1 > len)
			len1 = len;

		SDL_MixAudio(stream, audio_buff + audio_buf_index, len, SDL_MIX_MAXVOLUME);


		//memcpy(stream, (uint8_t*)(audio_buff + audio_buf_index), audio_buf_size);
		len -= len1;
		stream += len1;
		audio_buf_index += len1;
	}
}


int Player::createDisplay() {
	
	screen = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, videoCodecCtx->width, videoCodecCtx->height, SDL_WINDOW_OPENGL);
	if (!screen) {
		error_msg = (char *)"Could not create screen";
		return -1;
	}
	renderer = SDL_CreateRenderer(screen, -1, 0);
	bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STATIC, videoCodecCtx->width, videoCodecCtx->height);
	return 0;
}

void Player::quit() {

}

int sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
	SDL_Event event;
	event.type = FF_REFRESH_EVENT;
	event.user.data1 = opaque;
	SDL_PushEvent(&event);
	return 0;
}

static void schedule_refresh(PlayerState *ps, int delay) {
	SDL_AddTimer(delay, (SDL_TimerCallback)sdl_refresh_timer_cb, ps);
}

void video_refresh_timer(void *userdata) {

	PlayerState *ps = (PlayerState *)userdata;
	VideoPicture *vp;

	if (ps->video_st) {
		if (ps->pictq_size == 0) {
			schedule_refresh(is, 1);
		}
		else {
			vp = &is->pictq[is->pictq_rindex];
			schedule_refresh(is, 40);

			/* show the picture! */
			video_display(is);

			/* update queue for next picture! */
			if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
				is->pictq_rindex = 0;
			}
			SDL_LockMutex(is->pictq_mutex);
			is->pictq_size--;
			SDL_CondSignal(is->pictq_cond);
			SDL_UnlockMutex(is->pictq_mutex);
		}
	}
	else {
		schedule_refresh(is, 100);
	}
}

void Player::play() {

	SDL_Event event;
	pictq_mutex = SDL_CreateMutex();
	pictq_cond = SDL_CreateCond();
	SDL_AddTimer(videoFPS, (SDL_TimerCallback) sdl_refresh_timer_cb, ps);
	parse_tid = SDL_CreateThread(getFrames, "getFrame_thread", ps);

	while (true) {
		SDL_WaitEvent(&event);
		switch (event.type) {
		//case FF_QUIT_EVENT:
		case SDL_QUIT:
			quit();
			SDL_Quit();
			return;
		case FF_REFRESH_EVENT:
			//video_refresh_timer(event.user.data1);
			break;
		default:
			break;

		}
	}
}

int getFrames(void *args) {
	
	//PlayerState *ps = (PlayerState*)args;

	//AVPacket packet;

	////video context
	////sws_ctx = sws_getContext(ps->videoCodecCtx->width, ps->videoCodecCtx->height, ps->videoCodecCtx->pix_fmt, ps->videoCodecCtx->width, ps->videoCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
	//
	//SDL_Event evt;

	//int videoFPS = av_q2d(ps->pFormatCtx->streams[ps->videoStream]->r_frame_rate);

	//while (true) {
	//	av_read_frame(ps->pFormatCtx, &packet);
	//	
	//	if (packet.stream_index == ps->audioStream) {
	//		//putAudioPacket(&audioq, &packet);
	//	}

	//	if (packet.stream_index == ps->videoStream) {
	//		int res = avcodec_send_packet(ps->videoCodecCtx, &packet);
	//		if (res < 0) {
	//			//getAVError(res);
	//			continue;
	//		}
	//		res = avcodec_receive_frame(ps->videoCodecCtx, ps->pFrame);
	//		if (res < 0) {
	//			//getAVError(res);
	//			continue;
	//		}
	//		SDL_UpdateYUVTexture(ps->bmp, NULL, ps->pFrame->data[0], ps->pFrame->linesize[0], ps->pFrame->data[1], ps->pFrame->linesize[1], ps->pFrame->data[2], ps->pFrame->linesize[2]);
	//		SDL_RenderCopy(ps->renderer, ps->bmp, NULL, NULL);
	//		SDL_RenderPresent(ps->renderer);
	//		SDL_UpdateWindowSurface(ps->screen);
	//		SDL_Delay(videoFPS);
	//	}
	//	//av_packet_unref(&packet);
	//	SDL_PollEvent(&evt);
	//}

	return 1;

}
