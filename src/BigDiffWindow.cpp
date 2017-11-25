#include "BigDiffWindow.h"
#include "ui_BigDiffWindow.h"

struct BigDiffWindow::Private {
	TextEditorEnginePtr text_editor_engine;

};



BigDiffWindow::BigDiffWindow(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::BigDiffWindow)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::WindowMaximizeButtonHint;
	setWindowFlags(flags);

	m->text_editor_engine = TextEditorEnginePtr(new TextEditorEngine);

	ui->widget_diff->setMaximizeButtonEnabled(false);

	connect(ui->widget_diff, &FileDiffWidget::escPressed, [&](){
		close();
	});
}

BigDiffWindow::~BigDiffWindow()
{
	delete m;
	delete ui;
}

void BigDiffWindow::init(MainWindow *mw, FileDiffWidget::InitParam_ const &param)
{
	ui->widget_diff->bind(mw);

	switch (param.view_style) {
	case FileDiffWidget::ViewStyle::LeftOnly:
		ui->widget_diff->setLeftOnly(param.bytes_a, param.diff);
		break;
	case FileDiffWidget::ViewStyle::RightOnly:
		ui->widget_diff->setRightOnly(param.bytes_b, param.diff);
		break;
	case FileDiffWidget::ViewStyle::SideBySideText:
		ui->widget_diff->setSideBySide(param.bytes_a, param.diff, param.uncommited, param.workingdir);
		break;
	case FileDiffWidget::ViewStyle::SideBySideImage:
		ui->widget_diff->setSideBySide_(param.bytes_a, param.bytes_b, param.workingdir);
		break;
	}
}




