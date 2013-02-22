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

#include "Shot.h"
#include "CameraDS.h"
#include <iostream>
#include <highgui.h>
#include <cv.h>
#include <stdio.h>
#include <ctype.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qthread.h>
#include <qdatetime.h>
#include <qdebug.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qstring.h>
#include <stdlib.h>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\core\core.hpp>
#include <QtMultimedia/qaudioinput.h>
#include <QtMultimedia/qaudiooutput.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodeviceinfo.h>

#define fourcc 1145656920
#define fraction 4000

class Video: public QThread
{
	Q_OBJECT

private:
	CCameraDS camera;
	bool cameraOpen;
	int CameraID;
	Shot shot;
	int count;
	qint64 t1;
	qint64 t2;
	char* outFile;
	int linesize[8];
	uint8_t *data[8];
	IplImage* img;
	IplImage* cImg;
	bool isStart;
	bool restart;
	double fps;
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
	SwsContext *img_convert_ctx;
	double audioPts, videoPts;
	QAudioFormat format;
	QAudioDeviceInfo deviceInfo;
	QAudioInput* audioInput;
	QIODevice* device;
	AVFifoBuffer *fifo;
	uint8_t* samples;
	int buffer_size;
	int actual_buffer_size;
	int got_output, got_packet;
	int ret;

	void initialStream();
	void audioStart();
	void writeOneVideoFrame();
	void writeOneAudioFrame();
	void audioStop();
	void writeVideoEnding();
	void writeAudioEnding();
	void releaseStream();
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
		fifo = av_fifo_alloc(0);
		outFile = new char[100];
		count = 0;
		fps = 30;
		isStart = false;
		restart = false;
		cameraOpen = false;
		screenWidth = QApplication::desktop()->width();
		screenHeight = QApplication::desktop()->height();

		format.setFrequency(44100);
		format.setChannels(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);
		
		//initial ffmpeg
		av_register_all();
		avcodec_register_all();
		img_convert_ctx = sws_getContext(screenWidth,
										screenHeight,
										PIX_FMT_BGRA,
										screenWidth,
										screenHeight,
										PIX_FMT_YUV420P,
										SWS_BICUBIC, NULL,
										NULL,
										NULL);
		if (img_convert_ctx == NULL) {
			qDebug()<<"Cannot initialize the conversion context\n";
		}
	}
	~Video() {
		av_fifo_free(fifo);
		av_free(img_convert_ctx);
	}

	void run() {
		dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss-zzz").append(".avi");
		QByteArray routeFull = (route+"\\"+dateTime).toLocal8Bit();
		outFile = routeFull.data();
		
		if(cameraOpen) {
			camera.CloseCamera();
		}
		if(!camera.OpenCamera(CameraID, false, 320, 240)) {
			qDebug()<<"Open camera error";
		}
		cameraOpen = true;
		
		cImg = camera.QueryFrame();

		initialStream();
		audioStart();

		count = 0;
		restart = false;
		isStart = true;
		t1 = QDateTime::currentMSecsSinceEpoch();
		qDebug()<<"start";
		emit videoHasStarted();
		while (true) {
			t2 = QDateTime::currentMSecsSinceEpoch();
			if (t2 - t1 < 1000/fps*count) {
				writeOneAudioFrame();
				msleep(1);
				continue;
			}
			count++;
			if (count > fraction) {
				restart = true;
				isStart = false;
			}
			if (!isStart) break;
			
			qint64 it1 = QDateTime::currentMSecsSinceEpoch();
			
			cImg = camera.QueryFrame();
			if(!cImg) {
				qWarning() << "Can not get frame from the capture.";
				break;
			}
			img = shot.shot();
			if(!img) {
				qWarning() << "Can not get frame from the mirror driver.";
				break;
			}
			IplImage* cameraImg = cvCreateImage(cvSize(cImg->width, cImg->height), cImg->depth, img->nChannels);
			cvCvtColor(cImg, cameraImg, CV_BGR2BGRA);

			cvSetImageROI(img, cvRect(img->width - cameraImg->width, img->height - cameraImg->height, cameraImg->width, cameraImg->height));
			cvCopy(cameraImg, img);
			cvResetImageROI(img);

			linesize[0] = img->width*img->nChannels;
			data[0] = (uint8_t*)img->imageData;

			writeOneVideoFrame();

			cvReleaseImage(&cameraImg);
			delete img->imageData;
			cvReleaseImageHeader(&img);

			qint64 it2 = QDateTime::currentMSecsSinceEpoch();
			qDebug()<<"video: "<<it2 - it1;
		}
		audioStop();
		writeVideoEnding();
		writeAudioEnding();
		releaseStream();
		
		t2 = QDateTime::currentMSecsSinceEpoch();
		qDebug()<<t2 - t1 <<" "<< count;
		if (restart) emit videoRestart();
		emit outputFileName(QString(outFile));
	}
	void stop() {
		isStart = false;
	}
	void setSaveRoute(QString route) {
		this->route = route;
	}
	void setCamera(int id) {
		CameraID = id;
	}
signals:
	void videoRestart();
	void videoHasStarted();
	void outputFileName(const QString&);
};

#endif //VIDEO_H