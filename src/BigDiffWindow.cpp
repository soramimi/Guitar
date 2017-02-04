#include "BigDiffWindow.h"
#include "ui_BigDiffWindow.h"

BigDiffWindow::BigDiffWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::BigDiffWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	ui->widget_diff->setMaximizeButtonEnabled(false);
}

BigDiffWindow::~BigDiffWindow()
{
	delete ui;
}

void BigDiffWindow::init(MainWindow *mw, FileDiffWidget::InitParam_ const &param)
{
	ui->widget_diff->bind(mw);

	switch (param.view_style) {
	case FileDiffWidget::ViewStyle::TextLeftOnly:
		ui->widget_diff->setTextLeftOnly(param.content_left, param.diff);
		break;
	case FileDiffWidget::ViewStyle::TextRightOnly:
		ui->widget_diff->setTextRightOnly(param.content_left, param.diff);
		break;
	case FileDiffWidget::ViewStyle::TextSideBySide:
		ui->widget_diff->setTextSideBySide(param.content_left, param.diff, param.uncommited, param.workingdir);
		break;
	}
}
