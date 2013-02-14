#ifndef AUDIO_H
#define AUDIO_H

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

#include <QtGui/QMainWindow>
#include <qfile.h>
#include <QtMultimedia/qaudioinput.h>
#include <QtMultimedia/qaudiooutput.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <qbuffer.h>
#include <qtimer.h>
#include <qdebug.h>
#include <qpushbutton.h>
#include <qboxlayout.h>

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

//static QBuffer buffer;

class Buffer
{
public:
	AVFifoBuffer *fifo;
	int bytesAvailable;

	Buffer(QObject* parent = 0) {
		fifo = av_fifo_alloc(0);
	}
	~Buffer() {
		av_fifo_free(fifo);
	}

	qint64 writeData(char* data, qint64 maxSize) {
		if (av_fifo_space(fifo) < maxSize) {
			av_fifo_realloc2(fifo, av_fifo_size(fifo)+maxSize);
		}
		av_fifo_generic_write(fifo, data, maxSize, 0);
	}
	qint64 readData(char *data, qint64 maxlen) {

	}
};

class Audio : public QObject
{
	Q_OBJECT

public:
	Audio(QWidget *parent = 0): QObject(parent)
	{
		format.setFrequency(44100);
		format.setChannels(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);

		QIODevice* m_audioInputIODevice=audioInput->start();
		m_audioInputIODevice=audioInput->start();
		//connect(m_audioInputIODevice, SIGNAL(), this, SLOT(audioDataReady()));
	}
	~Audio() {
	}

public slots:
	void startRecording(){
		QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
		if (!info.isFormatSupported(format)) {
			qWarning()<<"default format not supported try to use nearest";
			format = info.nearestFormat(format);
		}
		qDebug()<<"f:"<<info.supportedFrequencies();
		qDebug()<<"sr:"<<info.supportedSampleRates();
		qDebug()<<"sz:"<<info.supportedSampleSizes();
		qDebug()<<"st:"<<info.supportedSampleTypes();
		qDebug()<<"bo:"<<info.supportedByteOrders();
		//outfile = fopen("test.sw", "wb");
		//buffer.open(QIODevice::ReadWrite);
		audioInput = new QAudioInput(info, format, this);
		//audioInput->start(&buffer);
	}
	void audioDataReady() {
		qDebug()<<"data ready:"<<audioInput->bytesReady();
	}
	void stopRecording()
	{
		audioInput->stop();
		delete audioInput;
	}
private:
	QAudioInput* audioInput;
	QAudioFormat format;
};

#endif // AUDIO_H