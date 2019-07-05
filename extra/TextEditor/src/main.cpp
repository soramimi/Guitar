#include "MainWindow.h"
#include <QApplication>
#include <QDebug>
#include "cmain.h"

QString makeApplicationDataDir();

int main(int argc, char *argv[])
{
	bool opt_qt = false;

	int i = 1;
	while (i < argc) {
		char const *arg = argv[i];
		if (*arg == '-') {
			if (strcmp(arg, "-qt") == 0) {
				opt_qt = true;
			}
		}
		i++;
	}

	QApplication a(argc, argv);

	if (opt_qt) {
		a.setOrganizationName("soramimi.jp");
		a.setApplicationName("ore");

		MainWindow w;
		w.show();

		return a.exec();
	} else {
		main2();
		return 0;
	}
}
