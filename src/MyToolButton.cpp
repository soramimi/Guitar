#include "MyToolButton.h"

#include "MainWindow.h"

#include <QPainter>

MyToolButton::MyToolButton(QWidget *parent)
	: QToolButton(parent)
{
}

void MyToolButton::setNumber(int n)
{
	number = n;
	update();
}

void MyToolButton::paintEvent(QPaintEvent *event)
{
	QToolButton::paintEvent(event);

	MainWindow *mw = qobject_cast<MainWindow *>(window());
	Q_ASSERT(mw);

	if (number >= 0) {
		int w = mw->digitWidth();
		int h = mw->digitHeight();
		QPixmap pm;
		{
			char tmp[100];
			int n = sprintf(tmp, "%u", number);

			pm = QPixmap((w + 1) * n + 3, h + 4);
			pm.fill(Qt::red);

			QPainter pr(&pm);
			for (int i = 0; i < n; i++) {
				mw->drawDigit(&pr, 2 + i * (w + 1), 2, tmp[i] - '0');
			}
		}
		QPainter pr(this);
		pr.drawPixmap(width() - pm.width(), 0, pm);
	}
}
