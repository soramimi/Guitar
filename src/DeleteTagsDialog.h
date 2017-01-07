#ifndef DELETETAGSDIALOG_H
#define DELETETAGSDIALOG_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class DeleteTagsDialog;
}

class DeleteTagsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DeleteTagsDialog(QWidget *parent, QList<Git::Tag> const &list);
	~DeleteTagsDialog();

	QStringList selectedTags() const;

private slots:
	void on_pushButton_check_all_clicked();

private:
	Ui::DeleteTagsDialog *ui;
};

#endif // DELETETAGSDIALOG_H
