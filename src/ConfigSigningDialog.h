#ifndef CONFIGSIGNINGDIALOG_H
#define CONFIGSIGNINGDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

class BasicMainWindow;

namespace Ui {
class ConfigSigningDialog;
}

class ConfigSigningDialog : public QDialog {
	Q_OBJECT

public:
	explicit ConfigSigningDialog(QWidget *parent, BasicMainWindow *mw, bool local_enable);
	~ConfigSigningDialog() override;

private:
	Ui::ConfigSigningDialog *ui;
	BasicMainWindow *mainwindow_;
	BasicMainWindow *mainwindow();
	Git::SignPolicy gpol_;
	Git::SignPolicy lpol_;

	void updateSigningInfo();

public slots:
	void accept() override;
};

#endif // CONFIGSIGNINGDIALOG_H
