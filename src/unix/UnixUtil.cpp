#include "UnixUtil.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QString>
#include "common/joinpath.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace platform {

void createApplicationShortcut(QWidget *parent)
{
	QString exec = QApplication::applicationFilePath();

	QString home = QDir::home().absolutePath();
	QString icon_dir = home / ".local/share/icons/jp.soramimi/";
	QString launcher_dir = home / ".local/share/applications/";
	QString name = "jp.soramimi.Guitar";
	QString iconfile = icon_dir / name + ".svg";
	QString launcher_path = launcher_dir / name + ".desktop";
	launcher_path = QFileDialog::getSaveFileName(parent, QApplication::tr("Save Launcher File"), launcher_path, "Launcher files (*.desktop)");

	bool ok = false;

	if (!launcher_path.isEmpty()) {
		QFile out(launcher_path);
		if (out.open(QFile::WriteOnly)) {
QString data = R"---([Desktop Entry]
Type=Application
Name=Guitar
Categories=Development
Exec=%1
Icon=%2
Terminal=false
)---";
			data = data.arg(exec).arg(iconfile);
			std::string s = data.toStdString();
			out.write(s.c_str(), s.size());
			out.close();
			std::string path = launcher_path.toStdString();
			chmod(path.c_str(), 0755);
			ok = true;
		}
	}

	if (ok) {
		QFile src(":/image/Guitar.svg");
		if (src.open(QFile::ReadOnly)) {
			QByteArray ba = src.readAll();
			src.close();
			QDir().mkpath(icon_dir);
			QFile dst(iconfile);
			if (dst.open(QFile::WriteOnly)) {
				dst.write(ba);
				dst.close();
			}
		}
	}
}

} // namespace platform
