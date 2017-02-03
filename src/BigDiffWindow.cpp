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

void BigDiffWindow::prepare(MainWindow *mw, FileDiffWidget::ViewStyle view_style, const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir)
{
	ui->widget_diff->bind(mw);

	switch (view_style) {
	case FileDiffWidget::ViewStyle::TextLeftOnly:
		ui->widget_diff->setDataAsDeletedFile(ba, diff);
		break;
	case FileDiffWidget::ViewStyle::TextRightOnly:
		ui->widget_diff->setDataAsAddedFile(ba, diff);
		break;
	case FileDiffWidget::ViewStyle::TextSideBySide:
		ui->widget_diff->setDiffData(ba, diff, uncommited, workingdir);
		break;
	}
}
