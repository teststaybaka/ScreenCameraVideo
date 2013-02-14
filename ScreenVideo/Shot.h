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
#include <highgui.h>
#include <cv.h>
#include <stdio.h>
#include <ctype.h>
#include <qdatetime.h>
#include <qdebug.h>
#include <qtimer.h>

class Shot
{
private:
	static const int EXT_DEVMODE_SIZE_MAX = 3072;
	struct DFEXT_DEVMODE : DEVMODE
	{
		char extension[EXT_DEVMODE_SIZE_MAX];
	};

	bool isDriverOpened;
	bool isDriverLoaded;
	bool isDriverAttached;
	bool isDriverConnected;

	HDC driverDC;
	DWORD deviceNumber;
	DISPLAY_DEVICE deviceInfo;
	DFEXT_DEVMODE deviceMode;
	RegistryKey regkeyDevice;
	CHANGES_BUF *changesBuffer;
	void *screenBuffer;

	BITMAP bitmap;
	RECT rect;
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
	Shot() : driverDC(0), screenBuffer(0), deviceNumber(0), isDriverOpened(0), isDriverLoaded(0), isDriverConnected(0) 
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

		memset(&deviceMode, 0, sizeof(deviceMode));
		deviceMode.dmSize = sizeof(DEVMODE);
		
		open();
		load();
		connect();
	}
	~Shot() {
		dispose();
	}
	void open();
	void close();

	void load();
	void unload();

	void connect();
	void disconnect();

	void dispose();

	IplImage* shot() {
		//qint64 t1 = QDateTime::currentMSecsSinceEpoch();

		IplImage* img = cvCreateImageHeader(cvSize(screenWidth,screenHeight), depth, nChannels);   
		BYTE *pBuffer = new BYTE[screenHeight*screenWidth*nChannels];   
		BYTE *bmBits = (BYTE*)screenBuffer;
		BYTE *temp = pBuffer;
		for (int i = 0; i < screenHeight; i++)
		{
			memcpy(temp, bmBits, screenWidth*nChannels);
			temp += screenWidth*nChannels;
			bmBits += screenWidth*nChannels;
		}
		cvSetData(img, pBuffer, screenWidth*nChannels);   

		//qint64 t2 = QDateTime::currentMSecsSinceEpoch();
		//qDebug()<<"shot:"<<t2-t1;
		return img;
	}
};

#endif //SCREENSHOT_H