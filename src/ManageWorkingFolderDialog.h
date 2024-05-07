#ifndef MANAGEWORKINGFOLDERDIALOG_H
#define MANAGEWORKINGFOLDERDIALOG_H

#include "ApplicationSettings.h"
#include <QDialog>

namespace Ui {
class ManageWorkingFolderDialog;
}

class ManageWorkingFolderDialog : public QDialog {
	Q_OBJECT
private:
	ApplicationSettings settings_;
public:
	explicit ManageWorkingFolderDialog(QWidget *parent = nullptr);
	~ManageWorkingFolderDialog();

private:
	Ui::ManageWorkingFolderDialog *ui;

	// QDialog interface
public slots:
	void accept();
};

#endif // MANAGEWORKINGFOLDERDIALOG_H
