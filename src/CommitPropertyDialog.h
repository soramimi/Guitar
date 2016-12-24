#ifndef COMMITPROPERTYDIALOG_H
#define COMMITPROPERTYDIALOG_H

#include <QDialog>

#include "Git.h"

namespace Ui {
class CommitPropertyDialog;
}

class CommitPropertyDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CommitPropertyDialog(QWidget *parent, Git::CommitItem const &commit);
	~CommitPropertyDialog();

private slots:

private:
	Ui::CommitPropertyDialog *ui;
};

#endif // COMMITPROPERTYDIALOG_H
