#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libpostproc/postprocess.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
}
#include <string>
#include <queue>

#include <SDL.h>
#ifdef main
#undef main
#endif

#define SDL_AUDIO_BUFFER_SIZE 1024
#define ERROR_SIZE 128
#define FF_REFRESH_EVENT (SDL_USEREVENT)

typedef struct _Packet{
	AVPacketList *first, *last;
	int nb_packets, size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} Packet;

typedef struct _VideoPicture {
	SDL_Texture *bmp;
	int width, height;
	int allocated;
} VideoPicture;

typedef struct _PlayerState {
	AVCodecContext *videoCodecCtx;
	AVFormatContext *pFormatCtx;
	int videoStream;
	int audioStream;
	AVFrame *pFrame;
	SDL_Window *screen;
	SDL_Renderer *renderer;
	SDL_Texture* bmp;
	AVStream *video_st;
	int pictq_size, pictq_rindex, pictq_windex;
} PlayerState;

class Player {
private:
	std::string error_msg;
	Packet *vidoePacket;
	int videoStream, audioStream;
	int videoFPS;
	AVFormatContext *pFormatCtx;
	AVCodecParameters *videoCodecParm;
	AVCodecParameters *audioCodecParm;
	AVCodec *videoCodec, *audioCodec;
	AVCodecContext *videoCodecCtx, *audioCodecCtx;
	AVFrame *pFrame, *pFrameRGB;
	struct SwsContext *sws_ctx ;
	uint8_t *buffer;
	SDL_Window *screen;
	SDL_Renderer *renderer;
	SDL_Texture* bmp;
	SDL_mutex *pictq_mutex;
	SDL_cond *pictq_cond;
	SDL_Thread *parse_tid;
	PlayerState *ps;

public:	
	
	Player();
	~Player();
	int openFile(std::string &filename);
	int getCodecParameters();
	int getCodec();
	void getInfo();
	std::string getAVError(int err);

	int allocateMemory();
	void initAudioPacket(Packet *pkt);
	void quit();
	static int getAudioPacket(Packet*, AVPacket*, int);
	int createDisplay();
	void play();

};