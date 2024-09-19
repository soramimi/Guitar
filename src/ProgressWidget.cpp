#include "ProgressWidget.h"
#include "ui_ProgressWidget.h"

ProgressWidget::ProgressWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::ProgressWidget)
{
	ui->setupUi(this);
}

ProgressWidget::~ProgressWidget()
{
	delete ui;
}

void ProgressWidget::setProgress(float progress)
{
	ui->label_bar->setProgress(progress);
}

void ProgressWidget::setText(const QString &text)
{
	ui->label_text->setText(text);
}

void ProgressWidget::clear()
{
	setText({});
	setProgress(0.0f);
	setVisible(false);
}
