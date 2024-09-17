#include "MergeDialog.h"
#include "ui_MergeDialog.h"

MergeDialog::MergeDialog(QString const &fastforward, const std::vector<QString> &labels, const QString &curr_branch_name, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::MergeDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	setFastForwardPolicy(fastforward);

	int select = 0;
	for (size_t i = 0; i < labels.size(); i++) {
		QString const &label = labels[i];
		ui->listWidget_from->addItem(label);
		if (label == curr_branch_name) {
			select = i;
		}
	}
	ui->listWidget_from->setCurrentRow(select);
}

MergeDialog::~MergeDialog()
{
	delete ui;
}

QString MergeDialog::getFastForwardPolicy() const
{
	if (ui->radioButton_ff_only->isChecked()) return "only";
	if (ui->radioButton_ff_no->isChecked()) return "no";
	return "default";
}

void MergeDialog::setFastForwardPolicy(QString const &ff)
{
	if (ff.compare("only", Qt::CaseInsensitive) == 0) {
		ui->radioButton_ff_only->click();
		return;
	}
	if (ff.compare("no", Qt::CaseInsensitive) == 0) {
		ui->radioButton_ff_no->click();
		return;
	}
	ui->radioButton_ff_default->click();
}

Git::MergeFastForward MergeDialog::ff(QString const &ff)
{
	if (ff.compare("only", Qt::CaseInsensitive) == 0) {
		return Git::MergeFastForward::Only;
	}
	if (ff.compare("no", Qt::CaseInsensitive) == 0) {
		return Git::MergeFastForward::No;
	}
	return Git::MergeFastForward::Default;
}

QString MergeDialog::mergeFrom() const
{
	QListWidgetItem *p = ui->listWidget_from->currentItem();
	return p ? p->text() : QString();
}

bool MergeDialog::isSquashEnabled() const
{
	return ui->checkBox_squash->isChecked();
}

void MergeDialog::on_listWidget_from_itemDoubleClicked(QListWidgetItem *item)
{
	(void)item;
	done(QDialog::Accepted);
}
