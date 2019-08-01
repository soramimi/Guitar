#include "MergeDialog.h"
#include "ui_MergeDialog.h"

MergeDialog::MergeDialog(QString const &fastforward, const std::vector<QString> &labels, const QString curr_branch_name, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::MergeDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	setFastForwardPolicy(fastforward);

	for (int i = 0; i < labels.size(); i++) {
		QString const &label = labels[i];
		ui->listWidget_from->addItem(label);
		if (label == curr_branch_name) {
			ui->listWidget_from->setCurrentRow(i);
		}
	}
}

MergeDialog::~MergeDialog()
{
	delete ui;
}

QString MergeDialog::getFastForwardPolicy() const
{
	if (ui->radioButton_force_ff) return "yes";
	if (ui->radioButton_force_no_ff) return "no";
	return "default";
}

void MergeDialog::setFastForwardPolicy(QString const &ff)
{
	if (ff.compare("yes", Qt::CaseInsensitive) == 0) {
		ui->radioButton_force_ff->click();
		return;
	}
	if (ff.compare("no", Qt::CaseInsensitive) == 0) {
		ui->radioButton_force_no_ff->click();
		return;
	}
	ui->radioButton_ff_default->click();
}

QString MergeDialog::mergeFrom() const
{
	QListWidgetItem *p = ui->listWidget_from->currentItem();
	return p ? p->text() : QString();
}

