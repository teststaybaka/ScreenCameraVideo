#ifndef SCREENVIDEO_H
#define SCREENVIDEO_H

#include <QtGui/QApplication>
#include <QMainWindow>
#include <qmessagebox.h>
#include <qlabel.h>
#include <qboxlayout.h>
#include <qpushbutton.h>
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
#include <qradiobutton.h>
#include <qprogressBar.h>
#include <qstring.h>
#include "Video.h"
#include "Transcode.h"

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
	MainWidget(QWidget *parent = 0, Qt::WFlags flags = 0):isStarting(0), isStart(0), QMainWindow(parent, flags) 
	{
		QTextCodec::setCodecForTr(QTextCodec::codecForName("system"));
		trayIconInitial();
		interfaceInitial();
		registHotKey();

		video = new Video();
		transcode = new Transcode();
		connect(routeChangeButton, SIGNAL(clicked()), this, SLOT(changeRoute()));
		connect(record, SIGNAL(clicked()), this, SLOT(startOrStop()));
		connect(quit, SIGNAL(clicked()), this, SLOT(quitApp()));
		connect(qApp, SIGNAL(hotKey(int, int)), this, SLOT(startOrStop()));
		connect(video, SIGNAL(videoRestart()), this, SLOT(videoStarting()));
		connect(video, SIGNAL(videoHasStarted()), this, SLOT(videoHasStarted()));
		connect(video, SIGNAL(outputFileName(const QString&)), this, SLOT(setTranscodeFile(const QString&)));
		connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
		connect(recordAction, SIGNAL(triggered()), this, SLOT(startOrStop()));
		connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
		connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));
		connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
		connect(quitAction, SIGNAL(triggered()), this, SLOT(quitApp()));
		connect(srcRouteChange, SIGNAL(clicked()), this, SLOT(srcChangeFile()));
		connect(dstRouteChange, SIGNAL(clicked()), this, SLOT(dstChangeFile()));
		connect(transcodeStart, SIGNAL(clicked()), this, SLOT(startTranscode()));
		connect(transcode, SIGNAL(progress(double)), this, SLOT(showProgress(double)));
	}
	~MainWidget() {
		GlobalDeleteAtom(id);
		UnregisterHotKey(this->winId(), id);
		delete video;
		delete transcode;
	}

	void closeEvent(QCloseEvent* e) {
		this->hide();
		QString titlec=tr("最小化");
		QString textc=tr("使用Ctrl+F2开始或停止录制");
		trayIcon->showMessage(titlec, textc, QSystemTrayIcon::Information, 5000);
		e->ignore();
	}
public slots:
	void setTranscodeFile(const QString& file) {
		srcRoute->setText(file);
		QString outputFile(file);
		outputFile.chop(4);
		outputFile.append(".mp4");
		dstRoute->setText(outputFile);
	}
	void srcChangeFile() {
		QString dir = QFileDialog::getOpenFileName(this, tr("Open Directory"), "/home", tr("Video (*.avi)"));
		if (dir != "") srcRoute->setText(dir);
	}
	void dstChangeFile() {
		QString dir = QFileDialog::getSaveFileName(this, tr("Save File"), "/home", tr("Video (*.avi *.mp4)"));
		if (dir != "") dstRoute->setText(dir);
	}
	void startTranscode() {
		progressBar->reset();
		std::pair<int, int> rsl = resolutionList[resolution->currentIndex()];
		transcode->setParameter(srcRoute->text(), dstRoute->text(), rsl.first, rsl.second);
		transcode->wait();
		transcode->start();
	}
	void showProgress(double ratio) {
		progressBar->setValue((progressBar->maximum()-progressBar->minimum())*ratio + progressBar->minimum());
	}
	void changeRoute() {
		QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
		if (dir != "") route->setText(dir);
	}
	void videoStarting() {
		if (isStarting) return;
		isStarting = true;
		record->setEnabled(false);
		video->setSaveRoute(route->text());
		video->setCamera(cameraDevice->currentIndex());
		video->wait();
		video->start();
	}
	void videoHasStarted() {
		isStart = true;
		isStarting = false;
		record->setText(tr("结束录制"));
		trayIcon->setIcon(*stopIcon);
		record->setEnabled(true);
	}
	void videoStop() {
		video->stop();
		video->wait();
		isStart = false;
		record->setText(tr("开始录制"));
		trayIcon->setIcon(*startIcon);
	}
	void startOrStop()
	{
		isStart? videoStop(): videoStarting();
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
	bool isStarting;
	Video* video;
	Transcode* transcode;
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu; 
	QAction *recordAction;
	QAction *minimizeAction;
	QAction *maximizeAction;
	QAction *restoreAction;
	QAction *quitAction;
	QTabWidget *tabWidget;
	QWidget *recordSettings;
	QWidget *transcodeSettings;
	QComboBox *resolution;
	QComboBox *cameraDevice;
	QList<std::pair<int, int>> resolutionList;
	QLineEdit *route;
	QLineEdit *srcRoute;
	QLineEdit *dstRoute;
	QPushButton* record;
	QPushButton* quit;
	QPushButton* routeChangeButton;
	QPushButton* srcRouteChange;
	QPushButton* dstRouteChange;
	QPushButton* transcodeStart;
	QProgressBar* progressBar;
	QIcon *startIcon, *stopIcon;
};

#endif // SCREENVIDEO_H
