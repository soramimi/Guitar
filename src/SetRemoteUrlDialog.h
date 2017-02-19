#ifndef SETREMOTEURLDIALOG_H
#define SETREMOTEURLDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class SetRemoteUrlDialog;
}

class SetRemoteUrlDialog : public QDialog
{
	Q_OBJECT
private:
	GitPtr g;
public:
	explicit SetRemoteUrlDialog(QWidget *parent = 0);
	~SetRemoteUrlDialog();

	void exec(GitPtr g);
private:
	Ui::SetRemoteUrlDialog *ui;
	void updateRemotesTable();

	// QDialog interface
public slots:
	void accept();
};

#endif // SETREMOTEURLDIALOG_H
