#include "ExperimentDialog.h"
#include "ui_ExperimentDialog.h"

#include "GitHubAPI.h"
#include "MemoryReader.h"
#include "webclient.h"

#include <QCryptographicHash>

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

