#include "DialogHeaderFrame.h"
#include "ApplicationGlobal.h"

#include <QPainter>

DialogHeaderFrame::DialogHeaderFrame(QWidget *parent)
	: QFrame(parent)
{
}

void DialogHeaderFrame::paintEvent(QPaintEvent *)
{
	QPainter pr(this);
	pr.fillRect(this->rect(), global->theme->dialog_header_frame_bg);
}
