#include "AboutDialog.h"
#include "ui_AboutDialog.h"

char const *Guitar_Version = "0.0.0";
int Guitar_Year = 2016;
char const *source_revision = "xxxxxxx";

AboutDialog::AboutDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	setWindowTitle(tr("About Guitar"));

	ui->label_title->setText(QString("Guitar, v%1 (%2)").arg(Guitar_Version).arg(source_revision));
	ui->label_copyright->setText(QString("Copyright (C) %1 S.Fuchita").arg(Guitar_Year));
	ui->label_twitter->setText("(@soramimi_jp)");
	QString t = QString("Qt %1").arg(qVersion());
#if defined(_MSC_VER)
	t += QString(", msvc=%1").arg(_MSC_VER);
#elif defined(__clang__)
	t += QString(", clang=%1.%2").arg(__clang_major__).arg(__clang_minor__);
#elif defined(__GNUC__)
	t += QString(", gcc=%1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#endif
	ui->label_qt->setText(t);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}

void AboutDialog::mouseReleaseEvent(QMouseEvent *)
{
	accept();
}
