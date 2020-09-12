#include "ColorDialog.h"
#include "ui_ColorDialog.h"

ColorDialog::ColorDialog(QWidget *parent, const QColor &color)
	: QDialog(parent)
	, ui(new Ui::ColorDialog)
{
	ui->setupUi(this);

	ui->widget_editor->bind(ui->widget_square);

	connect(ui->widget_editor, &ColorEditWidget::colorChanged, [&](QColor const &color){
		ui->widget_preview->setColor(color);
	});

	setColor(color);
}

ColorDialog::~ColorDialog()
{
	delete ui;
}

QColor ColorDialog::color() const
{
	return ui->widget_editor->color();
}

void ColorDialog::setColor(QColor const &color)
{
	ui->widget_editor->setColor(color);
}

































