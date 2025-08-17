#include "InputNewTagDialog.h"
#include "MainWindow.h"
#include "EditTagsDialog.h"
#include "ui_EditTagsDialog.h"

EditTagsDialog::EditTagsDialog(MainWindow *parent, GitCommitItem const *commit) :
	QDialog(parent),
	ui(new Ui::EditTagsDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	commit_ = commit;

	ui->lineEdit_commit_id->setText(commit_->commit_id.toQString());
	ui->lineEdit_message->setText(commit_->message);

	updateTagList();
}

EditTagsDialog::~EditTagsDialog()
{
	delete ui;
}

MainWindow *EditTagsDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

TagList EditTagsDialog::queryTagList()
{
	return mainwindow()->queryCurrentCommitTagList();
}

void EditTagsDialog::updateTagList()
{
	ui->listWidget->clear();
	TagList list = queryTagList();
	for (GitTag const &t : list) {
		auto item = new QListWidgetItem(t.name);
		ui->listWidget->addItem(item);
	}
	ui->listWidget->sortItems();
}

QStringList EditTagsDialog::selectedTags()
{
	QStringList list;
	int n = ui->listWidget->count();
	for (int i = 0; i < n; i++) {
		QListWidgetItem *item = ui->listWidget->item(i);
		if (item->isSelected()) {
			list.push_back(item->text());
		}
	}
	return list;
}

void EditTagsDialog::on_pushButton_add_clicked()
{
	InputNewTagDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QString text = dlg.text();
		mainwindow()->addTag(text);
		updateTagList();
	}
}

void EditTagsDialog::on_pushButton_delete_clicked()
{
	QStringList list = selectedTags();
	mainwindow()->deleteTags(list);
	updateTagList();
}

