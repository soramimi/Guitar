#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
#if 0
	QApplication a(argc, argv);
	MainWindow w;
	w.show();
	return a.exec();
#else

	MeCaSearch mecasearch;

	puts(mecasearch.convert_roman_to_katakana("wagahaihanekodearu!").c_str());


	return 0;
#endif
}
