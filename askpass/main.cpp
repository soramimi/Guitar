#include "AskPassDialog.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QString text;

	if (argc > 1) {
		text = argv[1];
	}

	AskPassDialog dlg(text);
	if (dlg.exec() == QDialog::Accepted) {
		puts(dlg.text().toStdString().c_str());
	}

	return 0;
}
