#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#define USE_Dfmirage
#define nUSE_UVNC

#include <Windows.h>
#include <tchar.h>
#include <assert.h>
#ifdef USE_UVNC
#include "videodriver.h"
#endif
#ifdef USE_Dfmirage
#include "DisplayEsc.h"
#include "RegistryKey.h"
#endif
#include <qpixmap.h>
#include <qapplication.h>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\core\core.hpp>
#include "highgui.h"
#include "cv.h"
#include <stdio.h>
#include <ctype.h>
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

#define MAXSIZE 5
extern IplImage* ShotImage[MAXSIZE];
extern int QueueHead;
extern int QueueTail;

class Shot:public  QThread 
{
	Q_OBJECT
private:
	static const int EXT_DEVMODE_SIZE_MAX = 3072;
	struct DFEXT_DEVMODE : DEVMODE
	{
		char extension[EXT_DEVMODE_SIZE_MAX];
	};

	bool isStart;
	bool isDriverOpened;
	bool isDriverLoaded;
	bool isDriverAttached;
	bool isDriverConnected;
#ifdef USE_Dfmirage
	HDC driverDC;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;
	DFEXT_DEVMODE deviceMode;
	RegistryKey regkeyDevice;
	CHANGES_BUF *changesBuffer;
	void *screenBuffer;
#endif
	BITMAP bitmap;
	RECT rect;
#ifdef USE_UVNC
	VIDEODRIVER *mydriver;
#endif
	int screenWidth;
	int screenHeight;
	int depth;
	int nChannels;

	static const TCHAR MINIPORT_REGISTRY_PATH[];
	void extractDeviceInfo(TCHAR *driverName);
	void openDeviceRegKey(TCHAR *miniportName);
	void initializeRegistryKey(HKEY rootKey, const TCHAR *entry, bool createIfNotExists, SECURITY_ATTRIBUTES *sa);
	bool tryOpenSubKey(HKEY key, const TCHAR *subkey, HKEY *openedKey, bool createIfNotExists, SECURITY_ATTRIBUTES *sa);
	void setAttachToDesktop(bool value);
	void commitDisplayChanges(DEVMODE *pdm);
public:
	Shot(QObject *parent = NULL) : QThread(parent)
#ifdef USE_Dfmirage
		, driverDC(0), screenBuffer(0), deviceNumber(0), isDriverOpened(0), isDriverLoaded(0), isDriverConnected(0) 
#endif
	{
		HWND hWnd;
		HDC hSrcDC;
		HDC hMemDC;
		HBITMAP hBitmap;

		hWnd = ::GetDesktopWindow();
		GetWindowRect(hWnd, &rect);
		screenWidth = rect.right - rect.left;
		screenHeight = rect.bottom - rect.top;
		hSrcDC = ::GetWindowDC(hWnd);
		hMemDC = ::CreateCompatibleDC(hSrcDC);
		hBitmap = ::CreateCompatibleBitmap(hSrcDC, screenWidth, screenHeight);
		::GetObject(hBitmap, sizeof(BITMAP), &bitmap);
		nChannels = bitmap.bmBitsPixel/8;
		depth = IPL_DEPTH_8U;

#ifdef USE_UVNC
		mydriver = new VIDEODRIVER;
		mydriver->VIDEODRIVER_start(0, 0, screenWidth, screenHeight, 0);
		mydriver->HardwareCursor();
#endif
#ifdef USE_Dfmirage
		memset(&deviceMode, 0, sizeof(deviceMode));
		deviceMode.dmSize = sizeof(DEVMODE);
		
		open();
		load();
		connect();
#endif
	}
	~Shot() {
#ifdef USE_UVNC
		mydriver->VIDEODRIVER_Stop();
		delete mydriver;
#endif
#ifdef USE_Dfmirage
		dispose();
#endif
	}
	void open();
	void close();

	void load();
	void unload();

	void connect();
	void disconnect();

	void dispose();

	void run() 
	{
		isStart = true;
		while (true) {
			if(!isStart)
				break;
			qint64 t1 = QDateTime::currentMSecsSinceEpoch();

			if(ShotImage[QueueTail] != NULL) {
				delete ShotImage[QueueTail]->imageData;
				cvReleaseImageHeader(&ShotImage[QueueTail]);
			}

#ifdef USE_UVNC
			ShotImage[QueueTail] = cvCreateImageHeader(cvSize(screenWidth,screenHeight), depth, nChannels);   
			BYTE *pBuffer = new BYTE[screenHeight*screenWidth*nChannels];   
			//GetBitmapBits(hBitmap, nHeight*nWidth*nChannels, pBuffer);
			BYTE *bmBits = (BYTE*)mydriver->myframebuffer;
			BYTE *temp = pBuffer;
			for (int i=0;i<screenHeight;i++)
			{
				memcpy(temp, bmBits, screenWidth*nChannels);
				temp += screenWidth*nChannels;
				bmBits += screenWidth*nChannels;
			}
			cvSetData(ShotImage[QueueTail], pBuffer, screenWidth*nChannels);   
			QueueTail = (QueueTail + 1)%MAXSIZE;
#endif

#ifdef USE_Dfmirage
			ShotImage[QueueTail] = cvCreateImageHeader(cvSize(screenWidth,screenHeight), depth, nChannels);   
			BYTE *pBuffer = new BYTE[screenHeight*screenWidth*nChannels];   
			BYTE *bmBits = (BYTE*)screenBuffer;
			BYTE *temp = pBuffer;
			for (int i = 0; i < screenHeight; i++)
			{
				memcpy(temp, bmBits, screenWidth*nChannels);
				temp += screenWidth*nChannels;
				bmBits += screenWidth*nChannels;
			}
			cvSetData(ShotImage[QueueTail], pBuffer, screenWidth*nChannels);   
			QueueTail = (QueueTail + 1)%MAXSIZE;
#endif

			qint64 t2 = QDateTime::currentMSecsSinceEpoch();
			qDebug()<<"shot:"<<t2-t1;
		}
	}
	void stop()
	{
		isStart = false;
	}
};

#endif //SCREENSHOT_H