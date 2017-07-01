#include "ExperimentDialog.h"
#include "ui_ExperimentDialog.h"

#include "GitHubAPI.h"

ExperimentDialog::ExperimentDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ExperimentDialog)
{
	ui->setupUi(this);
}

ExperimentDialog::~ExperimentDialog()
{
	delete ui;
}

void ExperimentDialog::on_pushButton_get_clicked()
{
	QString name = ui->lineEdit->text();
	GitHubAPI github;
	QImage image = github.avatarImage(name.toStdString());
	ui->label_image->setPixmap(QPixmap::fromImage(image));
}
