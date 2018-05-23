#ifndef SETGPGSIGNINGDIALOG_H
#define SETGPGSIGNINGDIALOG_H

#include <QDialog>
#include "gpg.h"

class MainWindow;

namespace Ui {
class SetGpgSigningDialog;
}

class SetGpgSigningDialog : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	MainWindow *mainwindow();

public:
	explicit SetGpgSigningDialog(QWidget *parent, const QString &repo, QString const &global_key_id, QString const &repository_key_id);
	~SetGpgSigningDialog();

	QString id() const;
	QString name() const;
	QString mail() const;
	bool isGlobalChecked() const;
	bool isRepositoryChecked() const;
private slots:
	void on_radioButton_global_clicked();

	void on_radioButton_repository_clicked();

	void on_pushButton_select_clicked();

	void on_pushButton_clear_clicked();

	void on_pushButton_configure_clicked();

private:
	Ui::SetGpgSigningDialog *ui;
	void setKey_(gpg::Data const &key);
	void setKey_(const QString &key_id);
};

#endif // SETGPGSIGNINGDIALOG_H
