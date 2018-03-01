#include "BigDiffWindow.h"
#include "ui_BigDiffWindow.h"

struct BigDiffWindow::Private {
	TextEditorEnginePtr text_editor_engine;
	FileDiffWidget::InitParam_ param;
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

	ui->widget_diff->setMaximizeButtonEnabled(false);

	connect(ui->widget_diff, &FileDiffWidget::textcodecChanged, [&](){ updateDiffView(); });

	connect(ui->widget_diff, &FileDiffWidget::escPressed, [&](){
		close();
	});
}

BigDiffWindow::~BigDiffWindow()
{
	delete m;
	delete ui;
}

void BigDiffWindow::setTextCodec(QTextCodec *codec)
{
	m->text_editor_engine = TextEditorEnginePtr(new TextEditorEngine);
	ui->widget_diff->setTextCodec(codec);
}

void BigDiffWindow::updateDiffView()
{
	ui->widget_diff->updateDiffView(m->param.diff, m->param.uncommited);
}

void BigDiffWindow::init(MainWindow *mw, FileDiffWidget::InitParam_ const &param)
{
	ui->widget_diff->bind(mw);
	m->param = param;

	switch (m->param.view_style) {
	case FileDiffWidget::ViewStyle::LeftOnly:
		ui->widget_diff->setLeftOnly(m->param.bytes_a, m->param.diff);
		break;
	case FileDiffWidget::ViewStyle::RightOnly:
		ui->widget_diff->setRightOnly(m->param.bytes_b, m->param.diff);
		break;
	case FileDiffWidget::ViewStyle::SideBySideText:
		ui->widget_diff->setSideBySide(m->param.bytes_a, m->param.diff, m->param.uncommited, m->param.workingdir);
		break;
	case FileDiffWidget::ViewStyle::SideBySideImage:
		ui->widget_diff->setSideBySide_(m->param.bytes_a, m->param.bytes_b, m->param.workingdir);
		break;
	}
}




