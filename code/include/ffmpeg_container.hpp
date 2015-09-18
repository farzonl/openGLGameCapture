#ifndef _FFMPEG_HPP_
#define _FFMPEG_HPP_

#define MAXSPSPPSBUFFERSIZE     1000

#include <string>
#include <queue>
#include "audioCapture.hpp"
#include "MsdkEncoder.hpp"

#ifdef __cplusplus

#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif

#include <stdint.h>
extern "C"
{
#endif

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h> //av_opt_set_int and others for audio
#include <libswresample/swresample.h>
#include <libswscale/swscale.h> //sws_scale, SWS_BICUBIC

#ifdef __cplusplus
} // extern "C"
#endif

using namespace userAudio;

struct pnts
{
	int64_t pts, dts;
	pnts(int64_t pts, int64_t dts) : pts(pts), dts(dts) {}
};

class FFMPEG_Writer
{
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
	AVFrame * audio_out_frame, *video_out_frame; 
	int audio_in_frame_sz;
	std::queue<pnts> timeStamps;
	public:
		FFMPEG_Writer() : oc(NULL), audio_st(NULL),video_st(NULL),audio_out_frame(NULL),
		video_out_frame(NULL),audio_in_frame_sz(0),timeStamps()
		{
			audio_out_frame = avcodec_alloc_frame();
			video_out_frame = avcodec_alloc_frame();
			timeStamps.push(pnts(0,0));
		}
		~FFMPEG_Writer() 
		{ 
			avcodec_free_frame(&audio_out_frame);
			avcodec_free_frame(&video_out_frame);
		}

		int open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st);
		int open_audio(AVFormatContext *oc, AVCodec *codec, AVStream *st);
		void write_audio(uint8_t* audio_src_data, int a_bufsize);
		void write_video(const mfxBitstream *pMfxBitstream);
		void close_video();
		void close_audio();
	private:

	friend class FFMPEG_Muxer;
};


class FFMPEG_Muxer
{
	inAudioCapture mic;
	MsdkEncoder    *encoder;
	std::string fileName;
	FFMPEG_Writer videoWriter;
	int videoBitRate, audioBitRate;
	AVFormatContext *oc;
	AVOutputFormat *fmt;
	AVStream *audio_st, *video_st;
	AVCodec *audio_codec, *video_codec;
	double audio_time, video_time;
	public:
		FFMPEG_Muxer(inAudioCapture &mic,MsdkEncoder *encoder, std::string fileName = "test.mp4");
		~FFMPEG_Muxer();
		void test();
		void test2();
	private:
		int addStreams();
		AVStream* addStream(AVFormatContext *oc, AVCodec **codec,enum AVCodecID &codec_id);
		int openStreams();
};


#endif