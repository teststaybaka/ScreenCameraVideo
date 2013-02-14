#include "MainWidget.h"

void MainWidget::trayIconInitial() {
	//QIcon icon = QIcon(":/ScreenVideo/MyResources/icon.jpg");
	//setWindowIcon(icon);
	trayIcon = new QSystemTrayIcon(this);
	//trayIcon->setIcon(icon);
	trayIcon->setToolTip("Press ctrl+F2 to start/stop record.");
	recordAction = new QAction(tr("开始录制/停止录制 (Ctrl+F2)"), this);
	minimizeAction = new QAction(tr("最小化 (&I)"), this);
	maximizeAction = new QAction(tr("最大化 (&X)"), this);
	restoreAction = new QAction(tr("还原 (&R)"), this);
	quitAction = new QAction(tr("退出 (&Q)"), this);
	trayIconMenu = new QMenu(this);
	trayIconMenu->addAction(recordAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(minimizeAction);
	trayIconMenu->addAction(maximizeAction);
	trayIconMenu->addAction(restoreAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(quitAction);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->show();
}

void MainWidget::interfaceInitial() {
	tabWidget = new QTabWidget(this);
	setCentralWidget(tabWidget);
	baseSettings = new QWidget(this);
	advancedSettings = new QWidget(this);
	tabWidget->addTab(baseSettings, tr("基本设置"));
	tabWidget->addTab(advancedSettings, tr("高级设置"));

	QLabel* resolutionLabel = new QLabel(tr("录制分辨率:"), this);
	resolutionLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
	resolution = new QComboBox(this);
	resolutionList.append(std::make_pair(1024, 768));
	resolutionList.append(std::make_pair(1024, 576));
	for(int i = 0; i < resolutionList.size(); i++) {
		resolution->addItem(QString("%1 x %2").arg(resolutionList[i].first).arg(resolutionList[i].second));
	}
	resolution->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

	QLabel* audioLabel = new QLabel(tr("录音设备:  "), this);
	audioLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
	audioDevice = new QComboBox(this);
	deviceList = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
	foreach(QAudioDeviceInfo deviceInfo, deviceList) {
		audioDevice->addItem(deviceInfo.deviceName());
	}
	audioDevice->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

	QLabel* routeLabel = new QLabel(tr("保存路径:  "), this);
	routeLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
	route = new QLineEdit(this);
	route->setText("movies");
	route->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
	route->setReadOnly(true);
	routeChangeButton = new QPushButton(tr("更改"), this);
	routeChangeButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

	QSpacerItem* spacer = new QSpacerItem(100, 10, QSizePolicy::Expanding, QSizePolicy::Preferred);
	record = new QPushButton(tr("开始录制"), this);
	record->setAutoDefault(true);
	record->setDefault(true);
	record->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	quit = new QPushButton(tr("    退出    "), this);
	quit->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

	QHBoxLayout* hBox1 = new QHBoxLayout(this);
	hBox1->addWidget(resolutionLabel);
	hBox1->addWidget(resolution);
	QHBoxLayout* hBox2 = new QHBoxLayout(this);
	hBox2->addWidget(audioLabel);
	hBox2->addWidget(audioDevice);
	QHBoxLayout* hBox3 = new QHBoxLayout(this);
	hBox3->addWidget(routeLabel);
	hBox3->addWidget(route);
	hBox3->addWidget(routeChangeButton);
	QHBoxLayout* hBox4 = new QHBoxLayout(this);
	hBox4->addSpacerItem(spacer);
	hBox4->addWidget(record);
	hBox4->addWidget(quit);
	QVBoxLayout* vBox = new QVBoxLayout(this);
	vBox->addLayout(hBox1);
	vBox->addLayout(hBox2);
	vBox->addLayout(hBox3);
	vBox->addLayout(hBox4);
	baseSettings->setLayout(vBox);
}

void MainWidget::registHotKey() {
	HWND hWnd = this->winId();
	id = GlobalAddAtom(LPCWSTR("myHotKey"));
	UINT fsModifiers = MOD_CONTROL;
	UINT vk = VK_F2;
	if (!RegisterHotKey(hWnd, id, fsModifiers, vk)) 
	{
		QMessageBox::information(this, "Wrong", "Can't regist hot key!");
		this->close();
	}
}