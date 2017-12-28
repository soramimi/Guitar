#ifndef REFLOGDIALOG_H
#define REFLOGDIALOG_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class ReflogDialog;
}

class ReflogDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ReflogDialog(QWidget *parent, const Git::ReflogItemList &reflog);
	~ReflogDialog();

private:
	Ui::ReflogDialog *ui;
	void updateTable(const Git::ReflogItemList &reflog);
};

#endif // REFLOGDIALOG_H
