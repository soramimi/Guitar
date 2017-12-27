#ifndef SETGLOBALUSERDIALOG_H
#define SETGLOBALUSERDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class SetGlobalUserDialog;
}

class SetGlobalUserDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SetGlobalUserDialog(QWidget *parent = 0);
	~SetGlobalUserDialog();

	Git::User user() const;
private:
	Ui::SetGlobalUserDialog *ui;
};

#endif // SETGLOBALUSERDIALOG_H
