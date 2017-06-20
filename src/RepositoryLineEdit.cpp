#include "RepositoryLineEdit.h"

#include <QDebug>
#include <QDropEvent>
#include <QMimeData>

RepositoryLineEdit::RepositoryLineEdit(QWidget *parent)
	: QLineEdit(parent)
{

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


