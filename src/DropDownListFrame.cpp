#include "DropDownListFrame.h"

#include <QKeyEvent>
#include <QListWidget>
#include <QVBoxLayout>
#include <QApplication>
#include <QPainter>

DropDownListFrame::DropDownListFrame(QWidget *parent)
	: QFrame(parent)
{
	qApp->installEventFilter(this);
	installEventFilter(this);

	setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
	auto *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
	listw_ = new QListWidget;
	layout->addWidget(listw_);
}

void DropDownListFrame::addItem(const QString &text)
{
	listw_->addItem(text);
}

void DropDownListFrame::setItems(const QStringList &list)
{
	listw_->clear();
	for (QString const &s : list) {
		listw_->addItem(s);
	}
}

void DropDownListFrame::show()
{
	QWidget *par = parentWidget();
	QPoint pt = par->geometry().bottomLeft();
	pt = qobject_cast<QWidget *>(par->parent())->mapToGlobal(pt);
	int itemheight = listw_->visualItemRect(listw_->item(0)).height();
	int height = itemheight * listw_->count() + 4;
	listw_->setFixedHeight(height);
	setFixedHeight(height);
	setGeometry(pt.x(), pt.y(), par->geometry().width(), height);
	QFrame::show();
	activateWindow();
	listw_->setFocus();
	listw_->setCurrentRow(0);
}

void DropDownListFrame::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Escape:
		hide();
		return;
	case Qt::Key_Return:
		emit itemClicked(listw_->currentItem()->text());
		hide();
		return;
	}
	QFrame::keyPressEvent(event);
}



void DropDownListFrame::focusOutEvent(QFocusEvent *event)
{
	hide();
	event->accept();
}

bool DropDownListFrame::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::KeyPress:
		if (isVisible()) {
			QKeyEvent *e = static_cast<QKeyEvent *>(event);
			if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
				hide();
				return true;
			}
		}
		break;
	case QEvent::WindowDeactivate:
		if (watched == this) {
			hide();
			return true;
		}
		break;
	}
	return QFrame::eventFilter(watched, event);
}
