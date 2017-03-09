#include "MainWindow.h"
#include <QApplication>
#include "MySettings.h"
#include "main.h"
#include <string>
#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QProxyStyle>
#include <QTranslator>
#include "LegacyWindowsStyleTreeControl.h"
#include "webclient.h"

QString application_data_dir;
QColor panel_bg_color;

class MyStyle : public QProxyStyle {
private:
	LegacyWindowsStyleTreeControl legacy_windows_;
public:
	MyStyle()
		: QProxyStyle(0)
	{
	}
	void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const
	{
		if (element == QStyle::PE_IndicatorBranch) {
			if (legacy_windows_.drawPrimitive(element, option, painter, widget)) {
				return;
			}
		}
		QProxyStyle::drawPrimitive(element, option, painter, widget);
	}
};

//void test();

int main(int argc, char *argv[])
{
	WebClient::initialize();

	QApplication a(argc, argv);
	QStyle *style = new MyStyle();
	QApplication::setStyle(style);

	bool f_open_here = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--open-here") {
			f_open_here = true;
		}
	}

	a.setOrganizationName(ORGANIZTION_NAME);
	a.setApplicationName(APPLICATION_NAME);

	application_data_dir = makeApplicationDataDir();
	if (application_data_dir.isEmpty()) {
		QMessageBox::warning(0, qApp->applicationName(), "Preparation of data storage folder failed.");
		return 1;
	}

	QTranslator translator;
	{
#if defined(Q_OS_MACX)
		QString path = "../Resources/Guitar_ja";
#else
		QString path = "Guitar_ja";
#endif
		translator.load(path, a.applicationDirPath());
		a.installTranslator(&translator);
	}

	MainWindow w;
	panel_bg_color = w.palette().color(QPalette::Background);
	w.setWindowIcon(QIcon(":/image/guitar.png"));
	w.show();

	if (f_open_here) {
		QString dir = QDir::current().absolutePath();
		w.autoOpenRepository(dir);
	}

	return a.exec();
}
