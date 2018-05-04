#ifndef SETGPGVERIFICATIONDIALOG_H
#define SETGPGVERIFICATIONDIALOG_H

#include <QDialog>
#include "gpg.h"


namespace Ui {
class SetGpgVerificationDialog;
}

class SetGpgVerificationDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SetGpgVerificationDialog(QWidget *parent, const QString &repo);
	~SetGpgVerificationDialog();


	QString id() const;
	QString name() const;
	QString mail() const;
	bool isGlobalChecked() const;
	bool isRepositoryChecked() const;
private slots:
	void on_pushButton_select_gpg_key_clicked();

	void on_radioButton_global_clicked();

	void on_radioButton_repository_clicked();

private:
	Ui::SetGpgVerificationDialog *ui;
	void setKey_(gpg::Key const &key);
};

#endif // SETGPGVERIFICATIONDIALOG_H
