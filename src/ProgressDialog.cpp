#include "ProgressDialog.h"
#include "ui_ProgressDialog.h"

#include <QDebug>
#include <QKeyEvent>

ProgressDialog::ProgressDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::ProgressDialog)
{
	ui->setupUi(this);
#if 0
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags &= ~Qt::WindowCloseButtonHint;
	setWindowFlags(flags);
#else
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setFocusPolicy(Qt::NoFocus);
	setFixedSize(width(), 0);
#endif

	ui->widget_log->init(parent);

	ui->widget_log->setFocusAcceptable(false);
	ui->widget_log->setLeftBorderVisible(false);

	updateInterruptCounter();

	connect(this, SIGNAL(finish()), this, SLOT(accept()));
	connect(this, SIGNAL(writeLog(QString)), this, SLOT(writeLog_(QString)));
}

ProgressDialog::~ProgressDialog()
{
	delete ui;
}

void ProgressDialog::updateInterruptCounter()
{
	QString text;
	if (interrupt_counter < 2) {
		text = tr("Press Esc key to abort the process");
	} else {
		text = tr("Press Esc key %1 times to abort the process").arg(interrupt_counter);
	}
	ui->label_interrupt->setText(text);
}

void ProgressDialog::setLabelText(const QString &text)
{
	ui->label->setText(text);
}

void ProgressDialog::reject()
{
	// ignore
}

void ProgressDialog::interrupt()
{
	if (interrupt_counter > 1) {
		interrupt_counter--;
		updateInterruptCounter();
	} else {
		canceled_by_user = true;
		accept();
	}
}

void ProgressDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		interrupt();
	}
}

bool ProgressDialog::canceledByUser() const
{
	return canceled_by_user;
}

void ProgressDialog::writeLog_(QString text)
{
	ui->widget_log->termWrite(text);

	ui->widget_log->updateControls();
	ui->widget_log->scrollToBottom();
	ui->widget_log->update();
}


