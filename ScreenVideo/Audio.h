#ifndef AUDIO_H
#define AUDIO_H

#include <QtGui/QMainWindow>
#include <qfile.h>
#include <QtMultimedia/qaudioinput.h>
#include <QtMultimedia/qaudiooutput.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <qtimer.h>
#include <qdebug.h>
#include <qpushbutton.h>
#include <qboxlayout.h>

typedef struct
{
	char fccID[4]; //"RIFF"
	unsigned long dwSize;//length-8
	char fccType[4]; //"WAVE"
}HEADER;
typedef struct
{
	char fccID[4]; //"fmt "
	unsigned long dwSize; //16
	unsigned short wFormatTag; //1
	unsigned short wChannels; //1 or 2
	unsigned long dwSamplesPerSec; //44100
	unsigned long dwAvgBytesPerSec; //
	unsigned short wBlockAlign; //声道数*量化数/8
	unsigned short uiBitsPerSample; //量化数 8 or 16
}FMT;
typedef struct
{
	char fccID[4]; //"data"
	unsigned long dwSize; //length-44
}DATA;

class Audio : public QWidget
{
	Q_OBJECT

public:
	Audio(QWidget *parent = 0, Qt::WFlags flags = 0) 
	{
		connect(&outputFile, SIGNAL(readyRead()), this, SLOT(ready()));
	}
	~Audio() {}

public slots:
	void read() {
		qDebug()<<"read";
	}
	void startRecording(){
		outputFile.setFileName("test.raw");
		outputFile.open( QIODevice::WriteOnly | QIODevice::Truncate );

		QAudioFormat format;
		// set up the format you want, eg.
		format.setFrequency(8000);
		format.setChannels(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::UnSignedInt);

		QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
		if (!info.isFormatSupported(format)) {
			qWarning()<<"default format not supported try to use nearest";
			format = info.nearestFormat(format);
		}

		audioInput = new QAudioInput(format, this);
		audioInput->start(&outputFile);
	}
	void stopRecording()
	{
		audioInput->stop();
		outputFile.close();
		delete audioInput;
		qDebug()<<"finish";

		HEADER pcmHEADER;
		FMT pcmFMT;
		DATA pcmDATA;
		unsigned long m_pcmData;
		FILE *fp, *fpCopy;
		fp = fopen("test.raw", "rb");
		fpCopy = fopen("test.wav", "wb+");
		if(!fp)
		{
			qDebug() << "open pcm file error";
			return;
		}
		if(!fpCopy)
		{
			qDebug() << "create wave file error";
			return;
		}
		qstrcpy(pcmHEADER.fccID, "RIFF");
		qstrcpy(pcmHEADER.fccType, "WAVE");
		fseek(fpCopy, sizeof(HEADER), 1);
		pcmFMT.dwSamplesPerSec = 8000;
		pcmFMT.dwAvgBytesPerSec = pcmFMT.dwSamplesPerSec*sizeof(m_pcmData);
		pcmFMT.uiBitsPerSample = 16;
		qstrcpy(pcmFMT.fccID, "fmt ");
		pcmFMT.dwSize = 16;
		pcmFMT.wBlockAlign = 4;
		pcmFMT.wChannels = 2;
		pcmFMT.wFormatTag = 1;
		fwrite(&pcmFMT, sizeof(FMT), 1, fpCopy);
		qstrcpy(pcmDATA.fccID, "data");
		pcmDATA.dwSize = 0;
		fseek(fpCopy, sizeof(DATA), 1);
		fread(&m_pcmData, sizeof(unsigned long), 1, fp);
		while(!feof(fp))
		{
			pcmDATA.dwSize += 4; //计算数据的长度；每读入一个数据，长度就加一；
			fwrite(&m_pcmData, sizeof(unsigned long), 1, fpCopy); //将数据写入.wav文件;
			fread(&m_pcmData, sizeof(unsigned long), 1, fp); //从.pcm中读入数据
		}
		fclose(fp);
		pcmHEADER.dwSize = 44+pcmDATA.dwSize-8; //根据pcmDATA.dwsize得出pcmHEADER.dwsize的值
		rewind(fpCopy); //将fpCpy变为.wav的头，以便于写入HEADER和DATA;
		fwrite(&pcmHEADER, sizeof(HEADER), 1, fpCopy); //写入HEADER
		fseek(fpCopy, sizeof(FMT), 1);  //跳过FMT,因为FMT已经写入
		fwrite(&pcmDATA, sizeof(DATA), 1, fpCopy);   //写入DATA;
		fclose(fpCopy);
		}
	void startPlaying()
	{
		inputFile.setFileName("test.raw");
		inputFile.open(QIODevice::ReadOnly);

		QAudioFormat format;
		// Set up the format, eg.
		format.setFrequency(8000);
		format.setChannels(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::UnSignedInt);
		QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
		if (!info.isFormatSupported(format)) {
			qWarning()<<"raw audio format not supported by backend, cannot play audio.";
			return;
		}

		audioOutput = new QAudioOutput(format, this);
		//connect(audioOutput,SIGNAL(stateChanged(QAudio::State)),SLOT(finishedPlaying(QAudio::State)));
		audioOutput->start(&inputFile);
	}
	void stopPlaying()
	{
		audioOutput->stop();
		inputFile.close();
		delete audioOutput;
	}
private:
	QFile outputFile;
	QAudioInput* audioInput;
	QFile inputFile;          
	QAudioOutput *audioOutput;
};

#endif // AUDIO_H