#ifndef COMMITPROPERTYDIALOG_H
#define COMMITPROPERTYDIALOG_H

#include <QDialog>

#include "Git.h"

namespace Ui {
class CommitPropertyDialog;
}

class MainWindow;

class CommitPropertyDialog : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwin_;
	Git::CommitItem const *commit_;
public:
	explicit CommitPropertyDialog(QWidget *parent, MainWindow *mw, Git::CommitItem const *commit);
	~CommitPropertyDialog();

private slots:

	void on_pushButton_checkout_clicked();

private:
	Ui::CommitPropertyDialog *ui;
};

#endif // COMMITPROPERTYDIALOG_H
