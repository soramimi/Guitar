
#include "RepositoryLineEdit.h"
#include "common/misc.h"
#include <QDebug>
#include <QDropEvent>
#include <QMimeData>

RepositoryLineEdit::RepositoryLineEdit(QWidget *parent)
	: QLineEdit(parent)
{
	installEventFilter(this);
}

bool RepositoryLineEdit::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::FocusOut) {
		if (watched == this) {
			QString s = text();
			s = misc::complementRemoteURL(s, false);
			setText(s);
		}
	}
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *>(event);
		if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
			QString s = text();
			s = misc::complementRemoteURL(s, true);
			setText(s);
		}
	}
	return QLineEdit::eventFilter(watched, event);

}

void RepositoryLineEdit::dropEvent(QDropEvent *e)
{
	QMimeData const *mimedata = e->mimeData();
	QList<QUrl> urls = mimedata->urls();
	if (urls.size() == 1) {
		setText(urls[0].url());
		return;
	}
	if (mimedata->hasText()) {
		setText(mimedata->text());
		return;
	}
}


