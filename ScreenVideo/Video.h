#ifndef VIDEO_H
#define VIDEO_H

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

#include "CameraDS.h"
#include <iostream>
#include "highgui.h"
#include "cv.h"
#include <stdio.h>
#include <ctype.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qthread.h>
#include <qdatetime.h>
#include <qdebug.h>
#include <qimage.h>
#include <qcolor.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qchar.h>
#include <qstring.h>
#include <stdlib.h>
#include <string>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\core\core.hpp>

#define fourcc 1145656920
#define fraction 4000

#define MAXSIZE 5
extern IplImage* ShotImage[MAXSIZE];
extern int QueueHead;
extern int QueueTail;

class Video: public QThread
{
	Q_OBJECT

public:
	Video(QObject *parent = NULL) : QThread(parent) 
	{
		memset(linesize, 0, sizeof(int)*8);
		memset(data, 0, sizeof(uint8_t*)*8);
		oFormatCtx = 0;
		oCodec = 0;
		o2Codec = 0;
		frameYUV = 0;
		frameAUD = 0;
		outFile = new char[100];
		count = 0;
		fps = 25;
		isColor = 1;
		isStart = false;
		restart = false;
		screenWidth = QApplication::desktop()->width();
		screenHeight = QApplication::desktop()->height();
		if(!camera.OpenCamera(0, false, 320, 240)) {
			qDebug()<<"Open camera error";
		}
		//initial ffmpeg
		av_register_all();
		avcodec_register_all();
	}

	void run() {
		dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss-zzz").append(".mp4");
		QByteArray routeFull = dateTime.toLocal8Bit();
		outFile = routeFull.data();
		//writer.open("test.avi", fourcc/*CV_FOURCC('P','I','M','1')*/, fps, cvSize(screenWidth, screenHeight), true);
		//if (!writer.isOpened()) {
		//	qDebug()<<"Open writer wrong";
		//}
		cameraImg = camera.QueryFrame();

		avformat_alloc_output_context2(&oFormatCtx, NULL, "mp4", NULL);
		if (!oFormatCtx) {
			fprintf(stderr, "Memory error\n");
			exit(1);
		}
		ofmt = oFormatCtx->oformat;
		videoSt = NULL;
		audioSt = NULL;
		if (ofmt->video_codec != CODEC_ID_NONE) {
			oCodec = avcodec_find_encoder(ofmt->video_codec);
			if (!oCodec) {
				fprintf(stderr, "oCodec not found\n");
				exit(1);
			}
			videoSt = avformat_new_stream(oFormatCtx, oCodec);
			if (!videoSt) {
				fprintf(stderr, "Could not alloc stream\n");
				exit(1);
			}
			avcodec_get_context_defaults3(videoSt->codec, oCodec);
			videoSt->codec->codec_id = ofmt->video_codec;
			videoSt->codec->bit_rate = 400000;
			videoSt->codec->width = frameWidth;
			videoSt->codec->height = frameHeight;
			videoSt->codec->time_base.den = fps;
			videoSt->codec->time_base.num = 1;
			videoSt->codec->gop_size = 30;
			videoSt->codec->pix_fmt = PIX_FMT_YUV420P;
			videoSt->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		if (videoSt) {
			if (avcodec_open2(videoSt->codec, oCodec, NULL) < 0) {
				fprintf(stderr, "could not open video codec\n");
				exit(1);
			}
		}
		if (!(ofmt->flags & AVFMT_NOFILE)) {
			if (avio_open(&oFormatCtx->pb, outFile, AVIO_FLAG_WRITE) < 0) {
				printf("Could not open '%s'\n", outFile);
				exit(1);
			}
		}
		avformat_write_header(oFormatCtx, NULL);
		
		frameYUV = avcodec_alloc_frame();
		if(frameYUV==NULL) {
			qDebug()<<"alloc frame failed";
			exit(1);
		}
		// Determine required buffer size and allocate buffer
		avpicture_alloc((AVPicture*)frameYUV, PIX_FMT_YUV420P, 
							frameWidth, frameHeight);
		frameYUV->pts = 0;
		static struct SwsContext *img_convert_ctx;
		img_convert_ctx = sws_getContext(frameWidth,
										frameHeight,
										PIX_FMT_BGRA,
										frameWidth,
										frameHeight,
										PIX_FMT_YUV420P,
										SWS_BICUBIC, NULL,
										NULL,
										NULL);
		if (img_convert_ctx == NULL) {
			qDebug()<<"Cannot initialize the conversion context\n";
		}

		count = 0;
		restart = false;
		isStart = true;
		t1 = QDateTime::currentMSecsSinceEpoch();
		qint64 it1, it2;
		qDebug()<<"start";
		while (true) {
			if (count > fraction) {
				restart = true;
				isStart = false;
			}
			if (!isStart) break;
			it1 = QDateTime::currentMSecsSinceEpoch();
			count++;
			cameraImg = camera.QueryFrame();
			if(!cameraImg) {
				qWarning() << "Can not get frame from the capture.";
				break;
			}
			
			while(QueueHead == QueueTail) {
				msleep(1);
			}
			if(QueueHead != QueueTail) {
				QueueHead = (QueueTail + MAXSIZE - 1)%MAXSIZE;
				img = ShotImage[QueueHead];
			}
			IplImage* cImg = cvCreateImage(cvSize(cameraImg->width, cameraImg->height), cameraImg->depth, img->nChannels);
			cvCvtColor(cameraImg, cImg, CV_BGR2BGRA);

			IplImage* dest = cvCreateImage(cvSize(frameWidth, frameHeight), img->depth, img->nChannels);
			cvResize(img, dest, CV_INTER_LINEAR);
			//IplImage* dest = cvCreateImage(cvSize(frameWidth, frameHeight), img->depth, img->nChannels);
			//cvSetImageROI(dest, cvRect((dest->width - temp->width)/2, (dest->height - temp->height)/2, temp->width, temp->height));
			//cvCopy(temp, dest);
			//cvResetImageROI(dest);
			cvSetImageROI(dest, cvRect(dest->width - cImg->width, dest->height - cImg->height, cImg->width, cImg->height));
			cvCopy(cImg, dest);
			cvResetImageROI(dest);
			//cv::Mat res(dest, 0);
			//writer<<res;
			linesize[0] = dest->width*dest->nChannels;
			data[0] = (uint8_t*)dest->imageData;

			qint64 s1 = QDateTime::currentMSecsSinceEpoch();
			av_init_packet(&pktV);
			pktV.data = NULL;
			pktV.size = 0;
			sws_scale(img_convert_ctx,
						(const uint8_t*  const*)data,
						linesize,
						0,
						frameHeight,
						frameYUV->data,
						frameYUV->linesize);

			ret = avcodec_encode_video2(videoSt->codec, &pktV, frameYUV, &got_output);
			if (ret < 0) {
				fprintf(stderr, "error encoding frame\n");
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
				fprintf(stderr, "Error while writing video frame\n");
				exit(1);
			}
			frameYUV->pts++;
			//cvReleaseImage(&temp);
			cvReleaseImage(&dest);
			cvReleaseImage(&cImg);

			qint64 s2 = QDateTime::currentMSecsSinceEpoch();
			qDebug()<<"t "<<s2 - s1;
			it2 = QDateTime::currentMSecsSinceEpoch();
			qDebug()<<it2 - it1;
			while(it2 - it1 < 1000/fps) {
				msleep(1);
				it2 = QDateTime::currentMSecsSinceEpoch();
			}
		}
		//writer.release();
		got_output = 1;
		while(got_output) {
			av_init_packet(&pktV);
			pktV.data = NULL;
			pktV.size = 0;
			ret = avcodec_encode_video2(videoSt->codec, &pktV, NULL, &got_output);
			if (ret < 0) {
				fprintf(stderr, "error encoding frame\n");
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
				fprintf(stderr, "Error while writing video frame\n");
				exit(1);
			}
		}

		av_write_trailer(oFormatCtx);
		if (videoSt) {
			avcodec_close(videoSt->codec);
			av_free(frameYUV->data[0]);
			av_free(frameYUV);
		}
		if (!(ofmt->flags & AVFMT_NOFILE))
			/* Close the output file. */
			avio_close(oFormatCtx->pb);
		av_freep(&oFormatCtx->streams[0]->codec);
        av_freep(&oFormatCtx->streams[0]);
		av_free(oFormatCtx);
		av_free(img_convert_ctx);

		t2 = QDateTime::currentMSecsSinceEpoch();
		qDebug()<<t2 - t1 <<" "<< count;
		if (restart) {
			emit videoRestart();
		}
	}
	void stop() {
		isStart = false;
	}
	void setVideoSize(int width, int height) {
		frameWidth = width;
		frameHeight = height;
	}
	void setSaveRoute(QString route) {
		this->route = route;
	}
signals:
	void videoRestart();
private:
	CCameraDS camera;
	int count;
	qint64 t1;
	qint64 t2;
	char* outFile;
	cv::VideoWriter writer;
	int linesize[8];
	uint8_t *data[8];
	IplImage* img;
	IplImage* cameraImg;
	bool isStart;
	bool restart;
	int isColor;
	double fps;
	int frameWidth;
	int frameHeight;
	int screenWidth;
	int screenHeight;
	QString dateTime;
	QString route;
	AVFormatContext *oFormatCtx;
	AVCodec *oCodec, *o2Codec;
	AVFrame *frameYUV, *frameAUD;
	AVPacket pktV, pktA;
	AVOutputFormat* ofmt;
	AVStream *audioSt, *videoSt;
	double audioPts, videoPts;
	AVFifoBuffer *fifo;
	uint8_t* samples;
	int got_output;
	int ret;
};

#endif //VIDEO_H