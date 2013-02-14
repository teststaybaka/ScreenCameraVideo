#ifndef SCREENVIDEO_H
#define SCREENVIDEO_H

#include "Video.h"
#include <QtGui/QApplication>
#include <QMainWindow>
#include <qmessagebox.h>
#include <qlabel.h>
#include <qboxlayout.h>
#include <qpushbutton.h>
#include <Windows.h>
#include <qsystemtrayicon.h>
#include <qaction.h>
#include <qtextcodec.h>
#include <qmenu.h>
#include <qevent.h>
#include <qprocess.h>
#include <qtabwidget.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qfiledialog.h>
#include <qlist.h>

class GlobelHotKeyApplication: public QApplication
{
	Q_OBJECT

public:
	GlobelHotKeyApplication(int argc, char** argv): QApplication(argc, argv) { }

	bool winEventFilter(MSG *msg, long *result) {
		if (msg->message == WM_HOTKEY) {
			emit hotKey(HIWORD(msg->lParam), LOWORD(msg->lParam));
			return true;
		}
		return false;
	}

signals:
	void hotKey(int keyCode, int modifiers);
};

class MainWidget : public QMainWindow
{
	Q_OBJECT

public:
	MainWidget(QWidget *parent = 0, Qt::WFlags flags = 0):QMainWindow(parent, flags) 
	{
		QTextCodec::setCodecForTr(QTextCodec::codecForName("GB2312"));
		trayIconInitial();
		interfaceInitial();
		registHotKey();

		video = new Video();
		isStart = false;
		connect(routeChangeButton, SIGNAL(clicked()), this, SLOT(changeRoute()));
		connect(record, SIGNAL(clicked()), this, SLOT(startOrStop()));
		connect(quit, SIGNAL(clicked()), this, SLOT(quitApp()));
		connect(qApp, SIGNAL(hotKey(int, int)), this, SLOT(startOrStop()));
		connect(video, SIGNAL(videoRestart()), this, SLOT(videoStart()));
		connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
		connect(recordAction, SIGNAL(triggered()), this, SLOT(startOrStop()));
		connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
		connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));
		connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
		connect(quitAction, SIGNAL(triggered()), this, SLOT(quitApp()));
	}
	~MainWidget() {
		GlobalDeleteAtom(id);
		UnregisterHotKey(this->winId(), id);
	}

	void closeEvent(QCloseEvent* e) {
		this->hide();
		QString titlec=tr("最小化");
		QString textc=QString::fromLocal8Bit("使用Ctrl+F2开始或停止录制");
		trayIcon->showMessage(titlec, textc, QSystemTrayIcon::Information, 5000);
		e->ignore();
	}
public slots:
	void changeRoute() {
		QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
		route->setText(dir);
	}
	void videoStart() {
		record->setText(tr("结束录制"));
		isStart = true;
		std::pair<int, int> rsl = resolutionList[resolution->currentIndex()];
		video->setVideoSize(rsl.first, rsl.second);
		video->setSaveRoute(route->text());
		video->setAudioInfo(deviceList[audioDevice->currentIndex()]);
		video->wait();
		video->start();
	}
	void videoStop() {
		record->setText(tr("开始录制"));
		isStart = false;
		video->stop();
		video->wait();
	}
	void startOrStop() 
	{
		isStart? videoStop(): videoStart();
		if(isStart == false)
		{
			//			/	system("E:\\ScreenVideo\\ScreenVideo\\ScreenVideo\\ffmpeg.exe -i E:\\ScreenVideo\\ScreenVideo\\ScreenVideo\\test.avi -i E:\\ScreenVideo\\ScreenVideo\\ScreenVideo\\test.wav -vcodec copy -acodec copy E:\\ScreenVideo\\ScreenVideo\\ScreenVideo\\output.avi");
			//system("ffmpeg.exe -i test.avi -i test.wav -vcodec copy -acodec copy output.avi");
			//QProcess *myProcess = new QProcess();
			//myProcess->start(":/ScreenVideo/MyResources/ffmpeg.exe -i test.avi -i test.wav -vcodec copy -acodec copy output.avi");
			//myProcess->start("ffmpeg.exe -i test.avi -i test.wav -vcodec copy -acodec copy output.avi");
		}
	}
	void iconActivated(QSystemTrayIcon::ActivationReason reason) {
		if (reason == QSystemTrayIcon::DoubleClick) {
			this->showNormal();
		}
	}
	void quitApp() {
		if (isStart) {
			videoStop();
		}
		qApp->quit();
	}
private:
	void trayIconInitial();
	void interfaceInitial();
	void registHotKey();

	int id;
	bool isStart;
	Video* video;
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu; 
	QAction *recordAction;
	QAction *minimizeAction;
	QAction *maximizeAction;
	QAction *restoreAction;
	QAction *quitAction;
	QTabWidget *tabWidget;
	QWidget *baseSettings;
	QWidget *advancedSettings;
	QComboBox *resolution;
	QComboBox *audioDevice;
	QList<QAudioDeviceInfo> deviceList;
	QList<std::pair<int, int>> resolutionList;
	QLineEdit *route;
	QPushButton* record;
	QPushButton* quit;
	QPushButton* routeChangeButton;
};

#endif // SCREENVIDEO_H
