#include "DeleteTagsDialog.h"
#include "ui_DeleteTagsDialog.h"

DeleteTagsDialog::DeleteTagsDialog(QWidget *parent, const TagList &list)
	: QDialog(parent)
	, ui(new Ui::DeleteTagsDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	for (GitTag const &t : list) {
		auto item = new QListWidgetItem(t.name);
		item->setCheckState(Qt::Unchecked);
		ui->listWidget_tags->addItem(item);
	}
	ui->listWidget_tags->sortItems();
}

DeleteTagsDialog::~DeleteTagsDialog()
{
	delete ui;
}

QStringList DeleteTagsDialog::selectedTags() const
{
	QStringList tags;
	int n = ui->listWidget_tags->count();
	for (int i = 0; i < n; i++) {
		auto item = ui->listWidget_tags->item(i);
		if (item->checkState() == Qt::Checked) {
			QString name = item->text();
			tags.push_back(name);
		}
	}
	return tags;
}

void DeleteTagsDialog::on_pushButton_check_all_clicked()
{
	int n = ui->listWidget_tags->count();
	for (int i = 0; i < n; i++) {
		auto item = ui->listWidget_tags->item(i);
		item->setCheckState(Qt::Checked);
	}
}
