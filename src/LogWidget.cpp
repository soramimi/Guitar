#include "LogWidget.h"

LogWidget::LogWidget(QWidget *parent)
	: FileDiffWidget(parent)
{
	setFocusPolicy(Qt::NoFocus);
}

void LogWidget::init(MainWindow *mw)
{
	bind(mw);
	setTerminalMode();
	setMaximizeButtonEnabled(false);
}
