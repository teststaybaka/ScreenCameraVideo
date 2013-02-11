#include "Shot.h"
const TCHAR Shot::MINIPORT_REGISTRY_PATH[] =
  _T("SYSTEM\\CurrentControlSet\\Hardware Profiles\\")
  _T("Current\\System\\CurrentControlSet\\Services");

void Shot::open() {
	assert(!isDriverOpened);

	if (false && ::GetSystemMetrics(SM_REMOTESESSION)) {
		qDebug()<<"We're running in a Remote Desktop session. Can't operate in this environment!";
		return;
	}

	extractDeviceInfo(_T("Mirage Driver"));
	openDeviceRegKey(_T("dfmirage"));

	isDriverOpened = true;
}

void Shot::load() {
	assert(isDriverOpened);
	if (!isDriverLoaded) {
		setAttachToDesktop(true);
		driverDC = CreateDC(deviceInfo.DeviceName, 0, 0, 0);
		if (!driverDC) {
			qDebug()<<"Can't create device context on mirror driver";
			return;
		}
		qDebug()<<"Device context is created";

		isDriverLoaded = true;
		qDebug()<<"Mirror driver is now loaded";
	}
}

void Shot::setAttachToDesktop(bool value)
{
	if (!regkeyDevice.setValueAsInt32(_T("Attach.ToDesktop"),
		(int)value)) {
		qDebug() << "Can't set the Attach.ToDesktop.";
		return;
	}
	isDriverAttached = value;
}

void Shot::connect() {
	qDebug()<<"Try to connect to the mirror driver.";
	GETCHANGESBUF buf = {0};
	int res = ExtEscape(driverDC, dmf_esc_usm_pipe_map, 0, 0, sizeof(buf), (LPSTR)&buf);
	if (res <= 0) {
	  qDebug()<<"Can't set a connection for the mirror driver: ExtEscape() failed with "<<res;
	  return;
	}
	changesBuffer = buf.buffer;
	screenBuffer = buf.Userbuffer;

	isDriverConnected = true;
}

void Shot::extractDeviceInfo(TCHAR *driverName) {
	memset(&deviceInfo, 0, sizeof(deviceInfo));
	deviceInfo.cb = sizeof(deviceInfo);

	qDebug()<<"Searching for "<<driverName<<"...";

	deviceNumber = 0;
	bool result;
	while (result = EnumDisplayDevices(0, deviceNumber, &deviceInfo, 0)) {
		qDebug()<<"Found: "<<QString::fromWCharArray(deviceInfo.DeviceString);
		qDebug()<<"RegKey: "<<QString::fromWCharArray(deviceInfo.DeviceKey);
		StringStorage deviceString(deviceInfo.DeviceString);
		if (deviceString.isEqualTo(driverName)) {
			qDebug()<<QString::fromWCharArray(driverName)<<" is found";
			break;
		}
		deviceNumber++;
	}
	if (!result) {
		qDebug()<<"Can't find "<<driverName;
		return;
	}
}

void Shot::openDeviceRegKey(TCHAR *miniportName)
{
	StringStorage deviceKey(deviceInfo.DeviceKey);
	deviceKey.toUpperCase();
	TCHAR *substrPos = deviceKey.find(_T("\\DEVICE"));
	StringStorage subKey(_T("DEVICE0"));
	if (substrPos != 0) {
		StringStorage str(substrPos);
		if (str.getLength() >= 8) {
			str.getSubstring(&subKey, 1, 7);
		}
	}

	qDebug()<<"Opening registry key "<<MINIPORT_REGISTRY_PATH<<"\\"<<miniportName<<"\\"<<subKey.getString();

	RegistryKey regKeyServices(HKEY_LOCAL_MACHINE, MINIPORT_REGISTRY_PATH, true);
	RegistryKey regKeyDriver(&regKeyServices, miniportName, true);
	regkeyDevice.open(&regKeyDriver, subKey.getString(), true);
	if (!regKeyServices.isOpened() || !regKeyDriver.isOpened() ||
		!regkeyDevice.isOpened()) {
		qDebug()<<"Can't open registry for the mirror driver";
		return;
	}
}

void Shot::dispose()
{
  if (isDriverConnected) {
    disconnect();
  }
  if (isDriverLoaded) {
    unload();
  }
  if (isDriverOpened) {
    close();
  }
}

void Shot::disconnect() {
	qDebug() << "Try to disconnect the mirror driver.";
  if (isDriverConnected) {
		GETCHANGESBUF buf;
		buf.buffer = changesBuffer;
		buf.Userbuffer = screenBuffer;

    int res = ExtEscape(driverDC, dmf_esc_usm_pipe_unmap, sizeof(buf), (LPSTR)&buf, 0, 0);
    if (res <= 0) {
      qDebug()<<"Can't unmap buffer: error code = "<<res;
	  return;
    }
    isDriverConnected = false;
  }
}

void Shot::unload() {
	if (driverDC != 0) {
		DeleteDC(driverDC);
		driverDC = 0;
		qDebug()<<"The mirror driver device context released";
	}

	if (isDriverAttached) {
		qDebug()<<"Unloading mirror driver...";
		setAttachToDesktop(false);
	}
	isDriverLoaded = false;
}

void Shot::close() {
	regkeyDevice.close();
	isDriverOpened = false;
}