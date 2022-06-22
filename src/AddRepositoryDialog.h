#ifndef ADDREPOSITORYDIALOG_H
#define ADDREPOSITORYDIALOG_H

#include <QDialog>

namespace Ui {
class AddRepositoryDialog;
}

class MainWindow;

class AddRepositoryDialog : public QDialog {
	Q_OBJECT
private:
	Ui::AddRepositoryDialog *ui;

	enum SearchRepository {
		None,
		GitHub,
	};

	enum Mode {
		Clone,
		Initialize,
		AddExisting,
	};
	Mode mode_ = Clone;
	QString already_exists_;

	MainWindow *mainwindow();
	MainWindow const *mainwindow() const;
	QString defaultWorkingDir() const;

	AddRepositoryDialog::Mode mode() const;
	void validate(bool change_name);
	void setRemoteURL(const QString &url);
	void updateUI();
	QString browseLocalPath();
public:
	explicit AddRepositoryDialog(MainWindow *parent, QString const &dir = QString());
	~AddRepositoryDialog() override;

	QString localPath() const;
	QString name() const;
	QString remoteName() const;
	QString remoteURL() const;
	QString overridedSshKey();
private slots:
	void on_groupBox_remote_toggled(bool arg1);
	void on_lineEdit_bookmark_name_textChanged(QString const &arg1);
	void on_lineEdit_local_path_textChanged(QString const &arg1);
	void on_pushButton_test_repo_clicked();
	void on_radioButton_clone_clicked();
	void on_radioButton_init_clicked();
	void on_radioButton_add_existing_clicked();
	void on_comboBox_search_currentIndexChanged(int index);

	void on_lineEdit_remote_url_textChanged(const QString &arg1);

	void on_pushButton_browse_local_path_clicked();

public slots:
	void accept() override;
};

#endif // ADDREPOSITORYDIALOG_H
