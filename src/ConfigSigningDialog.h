#ifndef CONFIGSIGNINGDIALOG_H
#define CONFIGSIGNINGDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class ConfigSigningDialog;
}

class ConfigSigningDialog : public QDialog {
	Q_OBJECT

public:
	explicit ConfigSigningDialog(QWidget *parent, MainWindow *mw, bool local_enable);
	~ConfigSigningDialog() override;

private:
	Ui::ConfigSigningDialog *ui;
	MainWindow *mainwindow_;
	MainWindow *mainwindow();
	GitSignPolicy gpol_;
	GitSignPolicy lpol_;

	void updateSigningInfo();

public slots:
	void accept() override;
};

#endif // CONFIGSIGNINGDIALOG_H
