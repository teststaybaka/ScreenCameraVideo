#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <Windows.h>
#include <tchar.h>
#include <assert.h>
#include "DisplayEsc.h"
#include "RegistryKey.h"
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

	HDC driverDC;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;
	RegistryKey regkeyDevice;
	CHANGES_BUF *changesBuffer;
	void *screenBuffer;
	BITMAP bitmap;
	RECT rect;

	static const TCHAR MINIPORT_REGISTRY_PATH[];
	void extractDeviceInfo(TCHAR *driverName);
	void openDeviceRegKey(TCHAR *miniportName);
	void initializeRegistryKey(HKEY rootKey, const TCHAR *entry, bool createIfNotExists, SECURITY_ATTRIBUTES *sa);
	bool tryOpenSubKey(HKEY key, const TCHAR *subkey, HKEY *openedKey, bool createIfNotExists, SECURITY_ATTRIBUTES *sa);
	void setAttachToDesktop(bool value);
public:
	Shot(QObject *parent = NULL) : driverDC(0), screenBuffer(0), deviceNumber(0), isDriverOpened(0), isDriverLoaded(0), isDriverConnected(0), QThread(parent) {
		//open();
		//load();
		//connect();
	}
	~Shot() {
		//dispose();
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
		while(1)
		{
			if(!isStart)
				break;
			qint64 t1 = QDateTime::currentMSecsSinceEpoch();
			HWND hWnd;		
			int nWidth;
			int nHeight;
			HDC hSrcDC;
			HDC hMemDC;
			HBITMAP hBitmap;
			HBITMAP hOldBitmap;

			hWnd = ::GetDesktopWindow();
			GetWindowRect(hWnd, &rect);
			nWidth = rect.right - rect.left;
			nHeight = rect.bottom - rect.top;
			hSrcDC = ::GetWindowDC(hWnd);
			hMemDC = ::CreateCompatibleDC(hSrcDC);
			hBitmap = ::CreateCompatibleBitmap(hSrcDC, nWidth, nHeight);
			hOldBitmap = (HBITMAP)::SelectObject(hMemDC, hBitmap);
			::BitBlt(hMemDC, 0, 0, nWidth, nHeight, hSrcDC, 0, 0, SRCCOPY);
			::GetObject(hBitmap, sizeof(BITMAP), &bitmap);

			int depth = IPL_DEPTH_8U;  
			int nChannels = bitmap.bmBitsPixel/8;   

			if(ShotImage[QueueTail] != NULL) {
				delete ShotImage[QueueTail]->imageData;
				cvReleaseImageHeader(&ShotImage[QueueTail]);
			}

			ShotImage[QueueTail] = cvCreateImageHeader(cvSize(nWidth,nHeight), depth, nChannels);   
			BYTE *pBuffer = new BYTE[nHeight*nWidth*nChannels];   
			GetBitmapBits(hBitmap, nHeight*nWidth*nChannels, pBuffer); 
			cvSetData(ShotImage[QueueTail], pBuffer, nWidth*nChannels);
			//memcpy(img->imageData, pBuffer, nHeight*nWidth*nChannels);     

			
			//ShotImage[QueueTail] = cvCreateImage(cvGetSize(img), img->depth,3);      
			//cvCvtColor(img, ShotImage[QueueTail], CV_BGRA2BGR);   
			QueueTail = (QueueTail + 1)%MAXSIZE;

			::SelectObject(hMemDC, hOldBitmap);
			::DeleteObject(hBitmap);
			::DeleteDC(hMemDC);
			::ReleaseDC(hWnd, hSrcDC);
			//cvReleaseImageHeader(&img);
			//delete pBuffer;
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