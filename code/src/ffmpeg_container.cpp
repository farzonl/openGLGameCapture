#include "ffmpeg_container.hpp"
#include <iostream>
#include <exception>

static const char* formatType = "mp4";

static inline void addAudioStream(AVCodecContext *c, const WAVEFORMATEX pwfx,int bitRate);
static inline void addVideoStream(AVCodecContext *c,const mfxVideoParam &vParams,AVCodecID &codec_id, int bitRate);
void FFMPEG_Muxer::test()
{
	mic.startRecording();
	while(!(GetKeyState(VK_RETURN)&0x80)) 
	{
		audio_time = audio_st ? audio_st->pts.val * av_q2d(audio_st->time_base) : 0.0;
		video_time = video_st ? video_st->pts.val * av_q2d(video_st->time_base) : 0.0;
		
		//if( video_time > audio_time)
		{
			videoWriter.write_video(&encoder->getBitStream());
		}
		if(mic.writeProcCheck() == 0)
		{
			uint8_t* mic_data = (uint8_t*) mic.getVoiceData();
			videoWriter.write_audio(mic_data, mic.getDataSize() );
			mic.writeProcEnd();
		}
	}
}

void FFMPEG_Muxer::test2()
{
	static bool first= true;
	if(first)
	{
		mic.startRecording();
		first= false;
	}

	videoWriter.write_video(&encoder->getBitStream());

	if(mic.writeProcCheck() == 0)
	{
		uint8_t* mic_data = (uint8_t*) mic.getVoiceData();
		videoWriter.write_audio(mic_data, mic.getDataSize() );
		mic.writeProcEnd();
	}
}
FFMPEG_Muxer::~FFMPEG_Muxer()
{
	av_write_trailer(oc);
	if (video_st) videoWriter.close_video();
	if (audio_st) videoWriter.close_audio();

	if (!(fmt->flags & AVFMT_NOFILE)) /* Close the output file. */
		avio_close(oc->pb);
			
	/* free the stream */
	avformat_free_context(oc);
}
FFMPEG_Muxer::FFMPEG_Muxer(inAudioCapture &mic,MsdkEncoder *encoder, std::string fileName) :
	mic(mic), encoder(encoder), fileName(fileName), videoWriter(), audioBitRate(128)//65536,
	,videoBitRate(2000000), oc(NULL)
	//bitRate(encoder->getVPPParams().mfx.TargetKbps), GopRefDist(encoder->getVPPParams().mfx.GopRefDist)
{
	avcodec_register_all();
    av_register_all();
    avformat_network_init();  //not necessary for file-only transcode

	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, fileName.c_str());
	if (!oc) 
	{
		std::cout << " Output format picked failed trying mp4" << std::endl; 
		avformat_alloc_output_context2(&oc, NULL, formatType, fileName.c_str());
	}

	fmt = oc->oformat;

	addStreams();
	openStreams();
}

int FFMPEG_Muxer::openStreams()
{
	int ret = 0;
	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */

	if (video_st)
		videoWriter.open_video(oc, video_codec, video_st);

	if (audio_st)
		videoWriter.open_audio(oc, audio_codec, audio_st);

	av_dump_format(oc, 0, fileName.c_str(), 1);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) 
	{
		ret = avio_open(&oc->pb, fileName.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) 
		{
			std::cout << "Could not open" << fileName << " : error # "
				<< ret << std::endl; 
			return 1;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, NULL);
	if (ret < 0) 
	{
		std::cout << "Error occurred when opening output file: " <<
			ret<< std::endl; 
		return 1;
	}
}

int FFMPEG_Muxer::addStreams()
{
	// Add the audio and video streams using the default format codecs
	// and initialize the codecs. 
	video_st = NULL;
	audio_st = NULL;

	if (fmt->video_codec != AV_CODEC_ID_NONE) 
	{
		video_st = addStream(oc, &video_codec, fmt->video_codec);
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) 
	{
		audio_st = addStream(oc, &audio_codec, fmt->audio_codec);
	}

	return 0;
}

AVStream* FFMPEG_Muxer::addStream(AVFormatContext *oc, AVCodec **codec,enum AVCodecID &codec_id)
{
	AVCodecContext *c;
	AVStream *st;
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) 
	{
		std::cout <<"Could not find encoder for " << avcodec_get_name(codec_id) << std::endl;
		exit(1);
	}
	st = avformat_new_stream(oc, *codec);
	if (!st) 
	{
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	st->id = oc->nb_streams-1;
	c = st->codec;
	switch ((*codec)->type) 
	{
	case AVMEDIA_TYPE_AUDIO:
		addAudioStream(c,mic.getWAVFormat(),audioBitRate);
		break;
	case AVMEDIA_TYPE_VIDEO:
		addVideoStream(c,encoder->getVPPParams(),codec_id,videoBitRate);
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	return st;

}

static inline void addAudioStream(AVCodecContext *c, const WAVEFORMATEX pwfx,int bitRate)
{
	c->sample_fmt  =  AV_SAMPLE_FMT_S16;//NOTE: s16 is pcm input if fail try AV_SAMPLE_FMT_S16P
	c->bit_rate    = bitRate*1000;
	c->sample_rate = pwfx.nSamplesPerSec;
	c->channels    = pwfx.nChannels;
	//c->codec_id                 = AV_CODEC_ID_PCM_S16LE;
	c->bits_per_coded_sample	= pwfx.wBitsPerSample;
	//c->time_base                = (AVRational){1,c->sample_rate};
}

static inline void addVideoStream(AVCodecContext *c,const mfxVideoParam &vParams,AVCodecID &codec_id, int bitRate)
{
	c->codec_id       = codec_id;
	c->bit_rate       = bitRate; 
	c->width          = vParams.vpp.In.CropW;
	c->height         = vParams.vpp.In.CropH;
	c->time_base.den  = vParams.vpp.In.FrameRateExtN;
	c->time_base.num  = vParams.vpp.In.FrameRateExtD;
	c->gop_size       = 12; /* emit one intra frame every twelve frames at most */
	c->pix_fmt        = AV_PIX_FMT_YUV420P;

	//if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) 
	{
		/* just for testing, we also add B frames */
		//c->max_b_frames = 2;
	}
	//if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) 
	{
		/* Needed to avoid using macroblocks in which some coeffs overflow.
		* This does not happen with normal video, it just happens here as
		* the motion of the chroma plane does not match the luma plane. */
		//c->mb_decision = 2;
	}
}

int FFMPEG_Writer::open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
	int ret;
	this->video_st = st;
	if(!this->oc) this->oc = oc;
	AVCodecContext *c;
	c = st->codec;
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) 
	{
		std::cout << "Could not open video codec: "<< ret << std::endl;
		exit(1);
	}
	if (!video_out_frame) 
	{
		std::cout << "video frame is not allocate " << std::endl;
		exit(1);
	}
	return 0;
}

int FFMPEG_Writer::open_audio(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
	this->audio_st = st;
	if(!this->oc) this->oc = oc;
	
	AVCodecContext *c;
	int ret = 0;
	//st->duration = fmt_ctx->duration; // may need this
	c = st->codec;
	/* open it */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) 
	{
		//std::cout << "Could not open audio codec: "<< errorToStr(ret) << std::endl;
		std::cout << "Could not open audio codec: "<< ret << std::endl;
		exit(1);
	}
	if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE)
		audio_in_frame_sz = 10000;
    else
        audio_in_frame_sz = c->frame_size;

    //int tempSize = audio_in_frame_sz *
   //    av_get_bytes_per_sample(c->sample_fmt) *c->channels;

    return ret;
}


void FFMPEG_Writer::write_audio(uint8_t *audio_src_data, int a_bufsize)
{
	 if ( this->oc == NULL || audio_st == NULL ) return;

	AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
	int cur_pkt;

	
    c = audio_st->codec;
	audio_out_frame->nb_samples = audio_in_frame_sz;
	int buf_size = a_bufsize *av_get_bytes_per_sample(c->sample_fmt) *c->channels;
	uint8_t * a_src_cntr = audio_src_data;

	for(int i = 0; i < a_bufsize; i+=4*audio_in_frame_sz)
	{
		av_init_packet(&pkt);
		avcodec_fill_audio_frame(audio_out_frame, c->channels, c->sample_fmt,
			a_src_cntr, buf_size, 1);

		avcodec_encode_audio2(c, &pkt, audio_out_frame, &cur_pkt);

		if(cur_pkt)
		{
			if (pkt.pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(pkt.pts , audio_st->codec->time_base, audio_st->time_base);
        
			if (pkt.dts != AV_NOPTS_VALUE)
				pkt.dts = av_rescale_q(pkt.dts, audio_st->codec->time_base, audio_st->time_base);

			pkt.stream_index = audio_st->index;
			pkt.flags |= AV_PKT_FLAG_KEY;
			
			// Write the compressed frame to the media file. 
			try{
			if (av_interleaved_write_frame(oc, &pkt) != 0)
			{
				fprintf(stderr, "Error while writing audio frame\n");
				exit(1);
			}
			}
			catch(std::exception& e)
			{
				 std::cout << "An exception occurred. Exception Nr. " << e.what() << std::endl;
			}
		}
		
		a_src_cntr +=4*audio_in_frame_sz;
		timeStamps.push(pnts(pkt.pts,pkt.dts));
		av_free_packet(&pkt);
	}
	
	
}
void FFMPEG_Writer::write_video(const mfxBitstream *pMfxBitstream)
{
	 //MSDK_CHECK_POINTER(pMfxBitstream, MFX_ERR_NULL_PTR);
	if(NULL == pMfxBitstream) exit(1);

	if ( oc == NULL || video_st == NULL ) return;

	AVPacket pkt = {0};
	av_init_packet(&pkt);

	pkt.stream_index	= video_st->index;
    pkt.data			= pMfxBitstream->Data;
    pkt.size			= pMfxBitstream->DataLength;

	if(pMfxBitstream->FrameType == (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR))
        pkt.flags |= AV_PKT_FLAG_KEY;

	if(timeStamps.size() > 1)
	{
		pkt.pts = timeStamps.front().pts;
		pkt.dts = timeStamps.front().dts;
		timeStamps.pop();
	}
	else
	{
		pkt.pts = av_rescale_q(pkt.pts , video_st->codec->time_base, video_st->time_base);
		pkt.dts = av_rescale_q(pkt.dts,  video_st->codec->time_base, video_st->time_base);
	}
	if (av_interleaved_write_frame(oc, &pkt)) 
	{
        std::cout <<"FFMPEG: Error while writing video frame" << std::endl;
        exit(1);
    }
	av_free_packet(&pkt);
}

void FFMPEG_Writer::close_audio()
{
	avcodec_close(audio_st->codec);
}

void FFMPEG_Writer::close_video()
{
	avcodec_close(video_st->codec);
}