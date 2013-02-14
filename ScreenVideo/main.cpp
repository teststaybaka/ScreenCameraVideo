#include "MainWidget.h"

int main(int argc, char *argv[])
{
	GlobelHotKeyApplication a(argc, argv);
	MainWidget w;
	w.show();
	return a.exec();
}
