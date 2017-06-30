#ifndef CREATEREPOSITORYDIALOG_H
#define CREATEREPOSITORYDIALOG_H

#include <QDialog>

namespace Ui {
class CreateRepositoryDialog;
}

class MainWindow;

class CreateRepositoryDialog : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow_;
	bool remote_url_is_ok_ = false;
public:
	explicit CreateRepositoryDialog(MainWindow *parent = 0);
	~CreateRepositoryDialog();

	QString path() const;
	QString name() const;
	QString remoteURL() const;
private slots:
	void on_lineEdit_path_textChanged(const QString &arg1);
	void on_pushButton_browse_path_clicked();
	void on_lineEdit_name_textChanged(const QString &arg1);

	void on_groupBox_remote_toggled(bool arg1);

	void on_pushButton_test_repo_clicked();

private:
	Ui::CreateRepositoryDialog *ui;
	void validate();
};

#endif // CREATEREPOSITORYDIALOG_H
