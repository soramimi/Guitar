#ifndef CREATEREPOSITORYDIALOG_H
#define CREATEREPOSITORYDIALOG_H

#include <QDialog>

namespace Ui {
class CreateRepositoryDialog;
}

class BasicMainWindow;

class CreateRepositoryDialog : public QDialog
{
	Q_OBJECT
private:
	QString already_exists_;
public:
	explicit CreateRepositoryDialog(BasicMainWindow *parent, QString const &dir = QString());
	~CreateRepositoryDialog();

	QString path() const;
	QString name() const;
	QString remoteName() const;
	QString remoteURL() const;
private slots:
	void on_lineEdit_path_textChanged(QString const &arg1);
	void on_pushButton_browse_path_clicked();
	void on_lineEdit_name_textChanged(QString const &arg1);

	void on_groupBox_remote_toggled(bool arg1);

	void on_pushButton_test_repo_clicked();

private:
	Ui::CreateRepositoryDialog *ui;
	void validate(bool change_name);
	BasicMainWindow *mainwindow();

public slots:
	void accept();
};

#endif // CREATEREPOSITORYDIALOG_H
