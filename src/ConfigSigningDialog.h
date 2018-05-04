#ifndef CONFIGSIGNINGDIALOG_H
#define CONFIGSIGNINGDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class ConfigSigningDialog;
}

class ConfigSigningDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ConfigSigningDialog(QWidget *parent, MainWindow *mw, bool local_enable);
	~ConfigSigningDialog();

private:
	Ui::ConfigSigningDialog *ui;
	MainWindow *mainwindow_;
	MainWindow *mainwindow();
	Git::SignPolicy gpol_;
	Git::SignPolicy lpol_;

	void updateSigningInfo();

	// QDialog interface
public slots:
	void accept();
};

#endif // CONFIGSIGNINGDIALOG_H
