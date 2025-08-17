#ifndef EDITTAGSDIALOG_H
#define EDITTAGSDIALOG_H

#include "RepositoryModel.h"
#include "Git.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class EditTagsDialog;
}

class EditTagsDialog : public QDialog {
	Q_OBJECT
private:
	Ui::EditTagsDialog *ui;
	GitCommitItem const *commit_;
private:
	QStringList selectedTags();
	MainWindow *mainwindow();
	TagList queryTagList();
	void updateTagList();
public:
	explicit EditTagsDialog(MainWindow *parent, GitCommitItem const *commit);
	~EditTagsDialog() override;

private slots:
	void on_pushButton_add_clicked();
	void on_pushButton_delete_clicked();
};

#endif // EDITTAGSDIALOG_H
