#include "WorkingDirLineEdit.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QMimeData>
#include "common/misc.h"

WorkingDirLineEdit::WorkingDirLineEdit(QWidget *parent)
	: QLineEdit(parent)
{

}

void WorkingDirLineEdit::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
		return;
	}
	QLineEdit::dragEnterEvent(event);
}

void WorkingDirLineEdit::dropEvent(QDropEvent *event)
{
	QList<QUrl> urls = event->mimeData()->urls();
	if (urls.size() == 1) {
		QString path = urls[0].url();
		if (path.startsWith("file://")) {
			int i = 7;
#ifdef Q_OS_WIN
			if (path.utf16()[i] == '/') {
				i++;
			}
#endif
			path = path.mid(i);
			if (QFileInfo(path).isDir()) {
				path = misc::normalizePathSeparator(path);
				setText(path);
			}
		}
		return;
	}
	QLineEdit::dropEvent(event);
}
