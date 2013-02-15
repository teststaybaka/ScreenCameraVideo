#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libavcodec\avcodec.h>
#include <libavutil\avutil.h>
#include <libavutil\mathematics.h>
#include <libavdevice\avdevice.h>
#include <libavutil\opt.h>
#include <libavutil\avstring.h>
#include <libavutil\fifo.h>

#ifdef __cplusplus
}
#endif

#include <qthread.h>
#include <qstring.h>
#include <stdio.h>
#include <stdlib.h>

class Transcode: public QThread
{
	Q_OBJECT

private:
	AVFormatContext *iFormatCtx, *oFormatCtx;
	AVCodecContext* iCodecCtx, *i2CodecCtx;
	AVCodec* iCodec, *i2Codec, *oCodec, *o2Codec;
	AVFrame* frame, *frameYUV, *frameAUD;
	AVPacket packet, pktV, pktA;
	AVOutputFormat* ofmt;
	AVStream *audioSt, *videoSt;
	double audioPts, videoPts;
	AVFifoBuffer *fifo;
	int ret, got_picture, got, got_output, got_packet;
	uint8_t* samples;
	SwsContext *img_convert_ctx;

	QString inputFile;
	QString outputFile;
	int dstWidth;
	int dstHeight;
public:
	Transcode(QObject* parent = 0): QThread(parent) {
		iFormatCtx = 0;
		oFormatCtx = 0;
		iCodecCtx = 0;
		i2CodecCtx = 0;
		iCodec = 0;
		i2Codec = 0;
		oCodec = 0;
		o2Codec = 0;
		frame = 0;
		frameYUV = 0;
		frameAUD = 0;

		av_register_all();
		avcodec_register_all();
	}

	void setParameter(QString inputFile, QString outputFile, int dstWidth, int dstHeight) {
		this->inputFile = inputFile;
		this->outputFile = outputFile;
		this->dstWidth = dstWidth;
		this->dstHeight = dstHeight;
	}

	void run() {
		QByteArray inputRoute = inputFile.toLocal8Bit();
		char* inputFile = inputRoute.data();
		QByteArray outputRoute = outputFile.toLocal8Bit();
		char* outputFile = outputRoute.data();
		
		if (avformat_open_input(&iFormatCtx, inputFile, 0, 0) != 0)  {  
			qDebug("\n->(avformat_open_input)\tERROR:\t%d\n", ret);  
			return;
		}
		if (avformat_find_stream_info(iFormatCtx, 0) < 0)  {
			qDebug("\n->(avformat_find_stream_info)\tERROR:\t%d\n", ret);  
			return; 
		}
		long int duration = iFormatCtx->duration;
		int fps = iFormatCtx->streams[0]->time_base.den/iFormatCtx->streams[0]->time_base.num;
		int frameCount = 0;
		int totalFrame = duration*fps/1000000;

		iCodecCtx = iFormatCtx->streams[0]->codec;
		iCodec = avcodec_find_decoder(iCodecCtx->codec_id);
		if (!iCodec)  
		{  
			qDebug("\n->can not find codec!\n");  
			return;  
		}
		if (avcodec_open2(iCodecCtx, iCodec, NULL) < 0)
			return;

		i2CodecCtx = iFormatCtx->streams[1]->codec;
		i2Codec = avcodec_find_decoder(i2CodecCtx->codec_id);
		if (!i2Codec)  
		{  
			qDebug("\n->can not find codec!\n");  
			return;  
		}
		if (avcodec_open2(i2CodecCtx, i2Codec, NULL) < 0)
			return;
	
		//output settings:
		avformat_alloc_output_context2(&oFormatCtx, NULL, NULL, outputFile);
		if (!oFormatCtx) {
			qDebug("Memory error\n");
			exit(1);
		}
		ofmt = oFormatCtx->oformat;

		videoSt = NULL;
		audioSt = NULL;
		if (ofmt->video_codec != CODEC_ID_NONE) {
			//video_st = add_video_stream(oc, &video_codec, fmt->video_codec);
			oCodec = avcodec_find_encoder(ofmt->video_codec);
			if (!oCodec) {
				qDebug("oCodec not found\n");
				exit(1);
			}
			videoSt = avformat_new_stream(oFormatCtx, oCodec);
			if (!videoSt) {
				qDebug("Could not alloc stream\n");
				exit(1);
			}
			avcodec_get_context_defaults3(videoSt->codec, oCodec);
			videoSt->codec->codec_id = ofmt->video_codec;

			videoSt->codec->bit_rate = iCodecCtx->bit_rate;
			//videoSt->codec->bit_rate_tolerance = iCodecCtx->bit_rate_tolerance;
			videoSt->codec->width = dstWidth;
			videoSt->codec->height = dstHeight;
			videoSt->codec->time_base.den = iCodecCtx->time_base.den;
			videoSt->codec->time_base.num = iCodecCtx->time_base.num;
			videoSt->codec->gop_size = 30;//iCodecCtx->gop_size;
			videoSt->codec->pix_fmt = PIX_FMT_YUV420P;
			videoSt->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		if (ofmt->audio_codec != CODEC_ID_NONE) {
			o2Codec = avcodec_find_encoder(ofmt->audio_codec);
			if (!o2Codec) {
				qDebug("o2Codec not found\n");
				exit(1);
			}
			audioSt = avformat_new_stream(oFormatCtx, o2Codec);
			if (!audioSt) {
				qDebug("Could not alloc stream\n");
				exit(1);
			}
			audioSt->id = 1;

			audioSt->codec->bit_rate = i2CodecCtx->bit_rate;
			audioSt->codec->sample_rate = i2CodecCtx->sample_rate;//44100;
			audioSt->codec->sample_fmt = i2CodecCtx->sample_fmt;//AV_SAMPLE_FMT_S16;
			audioSt->codec->channels = 2;
			audioSt->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	
		if (videoSt) {
			if (avcodec_open2(videoSt->codec, oCodec, NULL) < 0) {
				qDebug("could not open video codec\n");
				exit(1);
			}
		}
		if (audioSt) {
			if (avcodec_open2(audioSt->codec, o2Codec, NULL) < 0) {
				qDebug("could not open audio codec\n");
				exit(1);
			}
		}
		av_dump_format(oFormatCtx, 0, outputFile, 1);
		if (!(ofmt->flags & AVFMT_NOFILE)) {
			if (avio_open(&oFormatCtx->pb, outputFile, AVIO_FLAG_WRITE) < 0) {
				qDebug("Could not open '%s'\n", outputFile);
				return;
			}
		}
		avformat_write_header(oFormatCtx, NULL);

		frame = avcodec_alloc_frame();
		frameYUV = avcodec_alloc_frame();
		if(frameYUV==NULL)
			return;
		// Determine required buffer size and allocate buffer
		avpicture_alloc((AVPicture*)frameYUV, PIX_FMT_YUV420P, 
							dstWidth, dstHeight);
		// other codes
		img_convert_ctx = sws_getContext(iCodecCtx->width,
										iCodecCtx->height, 
										iCodecCtx->pix_fmt,
										dstWidth,
										dstHeight,
										PIX_FMT_YUV420P,
										SWS_BICUBIC, NULL,
										NULL,
										NULL);

		frameAUD = avcodec_alloc_frame();
		frameAUD->nb_samples = audioSt->codec->frame_size;
		//frameAUD->format = audioSt->codec->sample_fmt;
		//frameAUD->channel_layout = audioSt->codec->channel_layout;
		int buffer_size = av_samples_get_buffer_size(NULL, audioSt->codec->channels,
													audioSt->codec->frame_size, 
													audioSt->codec->sample_fmt, 0);
		samples = (uint8_t*)av_malloc(buffer_size);
		fifo = av_fifo_alloc(0);
		av_init_packet(&packet);
		packet.size = 0;
		packet.data = NULL;
		frameYUV->pts = 0;
		while(av_read_frame(iFormatCtx, &packet) >= 0) {
			while(1) {
				if (packet.stream_index == 0) {
					frameCount++;
					emit progress((double)frameCount/totalFrame);

					if (avcodec_decode_video2(iCodecCtx, frame, &got_picture, &packet) < 0) {
						qDebug("Error while decoding\n");
					}
					if (got_picture)  
					{	
						av_init_packet(&pktV);
						pktV.data = NULL;
						pktV.size = 0;

						qDebug("[Video: type %d, ref %d, pts %lld, pkt_pts %lld, pkt_dts %lld]\n",   
								frame->pict_type, frame->reference, frame->pts, frame->pkt_pts, frame->pkt_dts);
						
						sws_scale(img_convert_ctx,
								(const uint8_t*  const*)frame->data,
								frame->linesize,
								0,
								iCodecCtx->height,
								frameYUV->data,
								frameYUV->linesize);
				
						ret = avcodec_encode_video2(videoSt->codec, &pktV, frameYUV, &got_output);
						if (ret < 0) {
							qDebug("error encoding frame\n");
							exit(1);
						}
						pktV.stream_index = videoSt->index;
						/* If size is zero, it means the image was buffered. */
						if (got_output) {
							if (pktV.pts != AV_NOPTS_VALUE)
								pktV.pts = av_rescale_q(pktV.pts, videoSt->codec->time_base, videoSt->time_base);
							if (pktV.dts != AV_NOPTS_VALUE)
								pktV.dts = av_rescale_q(pktV.dts, videoSt->codec->time_base, videoSt->time_base);
							if (videoSt->codec->coded_frame->key_frame)
								pktV.flags |= AV_PKT_FLAG_KEY;

							/* Write the compressed frame to the media file. */
							ret = av_interleaved_write_frame(oFormatCtx, &pktV);
							av_free_packet(&pktV);
						} else {
							ret = 0;
						}
						if (ret != 0) {
							qDebug("Error while writing video frame\n");
							exit(1);
						}
						frameYUV->pts++;
						break;
					}
				} else if (packet.stream_index == 1) {
					ret = avcodec_decode_audio4(i2CodecCtx, frame, &got, &packet);
					if (ret < 0) {
						qDebug("Error while decoding audio\n");
					}
					if (got)  
					{
						qDebug("[Audio: %5dB raw data]\n", frame->linesize[0]);

						av_init_packet(&pktA);
						pktA.data = NULL;
						pktA.size = 0;

						if (av_fifo_space(fifo) < frame->linesize[0]) {
							av_fifo_realloc2(fifo, av_fifo_size(fifo)+frame->linesize[0]);
						}
						av_fifo_generic_write(fifo, frame->data[0], frame->linesize[0], 0);
						if (av_fifo_size(fifo) < buffer_size)
							break;
						av_fifo_generic_read(fifo, samples, buffer_size, 0);
						ret = avcodec_fill_audio_frame(frameAUD, audioSt->codec->channels,
										audioSt->codec->sample_fmt,
										samples, buffer_size, 0);
						if (ret < 0) {
							qDebug("could not setup audio frame\n");
							exit(1);
						}
						ret = avcodec_encode_audio2(audioSt->codec, &pktA, frameAUD, &got_packet);
						if (ret < 0) {
							qDebug("error encoding audio frame\n");
							exit(1);
						}
						pktA.stream_index = audioSt->index;
						if (got_packet) {
							if (pktA.pts != AV_NOPTS_VALUE)
								pktA.pts = av_rescale_q(pktA.pts, audioSt->codec->time_base, audioSt->time_base);
							if (pktA.dts != AV_NOPTS_VALUE)
								pktA.dts = av_rescale_q(pktA.dts, audioSt->codec->time_base, audioSt->time_base);
							if (videoSt->codec->coded_frame->key_frame)
								pktA.flags |= AV_PKT_FLAG_KEY;

							if (av_interleaved_write_frame(oFormatCtx, &pktA) != 0) {
								qDebug("Error while writing audio frame\n");
								exit(1);
							}
							av_free_packet(&pktA);
						}
						break;
					}
				}
			}
			av_free_packet(&packet);
		}
		got_output = 1;
		while(got_output) {
			av_init_packet(&pktV);
			pktV.data = NULL;
			pktV.size = 0;
			ret = avcodec_encode_video2(videoSt->codec, &pktV, NULL, &got_output);
			if (ret < 0) {
				qDebug("error encoding frame\n");
				exit(1);
			}
			pktV.stream_index = videoSt->index;
			if (got_output) {
					/* Write the compressed frame to the media file. */
					ret = av_interleaved_write_frame(oFormatCtx, &pktV);
					av_free_packet(&pktV);
			} else {
				ret = 0;
			}
			if (ret != 0) {
				qDebug("Error while writing video frame\n");
				exit(1);
			}
		}
	
		while(av_fifo_size(fifo) > 0) {
			av_init_packet(&pktA);
			pktA.data = NULL;
			pktA.size = 0;

			av_fifo_generic_read(fifo, samples, buffer_size, 0);
			ret = avcodec_fill_audio_frame(frameAUD, audioSt->codec->channels,
										audioSt->codec->sample_fmt,
										samples, buffer_size, 0);
			if (ret < 0) {
				qDebug("could not setup audio frame\n");
				exit(1);
			}

			ret = avcodec_encode_audio2(audioSt->codec, &pktA, frameAUD, &got_packet);
			if (ret < 0) {
				qDebug("error encoding audio frame\n");
				exit(1);
			}
			pktA.stream_index = audioSt->index;
			if (got_packet) {
				if (av_interleaved_write_frame(oFormatCtx, &pktA) != 0) {
					qDebug("Error while writing audio frame\n");
					exit(1);
				}
				av_free_packet(&pktA);
			}
		}

		av_write_trailer(oFormatCtx);

		if (videoSt) {
			avcodec_close(videoSt->codec);
			av_free(frameYUV->data[0]);
			av_free(frameYUV);
		}
		if (audioSt) {
			avcodec_close(audioSt->codec);
			av_free(frameAUD->data[0]);
			av_free(frameAUD);
		}

		for (int i = 0; i < oFormatCtx->nb_streams; i++) {
			av_freep(&oFormatCtx->streams[i]->codec);
			av_freep(&oFormatCtx->streams[i]);
		}

		if (!(ofmt->flags & AVFMT_NOFILE))
			/* Close the output file. */
			avio_close(oFormatCtx->pb);

		av_freep(oFormatCtx);
		av_fifo_free(fifo);
		av_freep(img_convert_ctx);
		avcodec_close(iCodecCtx);
		avcodec_close(i2CodecCtx);
		avformat_close_input(&iFormatCtx);
	}

signals:
	void progress(double);
};