#include "MainWidget.h"

IplImage* ShotImage[MAXSIZE];
int QueueHead = 0;
int QueueTail = 0;

int main(int argc, char *argv[])
{
	GlobelHotKeyApplication a(argc, argv);
	MainWidget w;
	w.show();
	return a.exec();
}
