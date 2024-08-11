#ifndef ADDREPOSITORYDIALOG_H
#define ADDREPOSITORYDIALOG_H

#include "Git.h"
#include "RepositoryData.h"
#include <QDialog>

namespace Ui {
class AddRepositoryDialog;
}

class MainWindow;

class AddRepositoryDialog : public QDialog {
	Q_OBJECT
public:
	enum Mode {
		Clone,
		Initialize,
		AddExisting,
	};
private:
	Ui::AddRepositoryDialog *ui;

	enum SearchRepository {
		None,
		GitHub,
	};

	QString working_dir_;

	Mode mode_ = Clone;
	QString reponame_;
	QString already_exists_;

	MainWindow *mainwindow();
	MainWindow const *mainwindow() const;
	QString workingDir() const;

	void validate() const;
	void setRemoteURL(const QString &url);
	void browseLocalPath();
	void updateUI();
	void setWorkingDir(const QString &dir);
	void updateLocalPath();
	void updateWorkingDirComboBoxFolders();
	void parseAndUpdateRemoteURL();
public:
	explicit AddRepositoryDialog(MainWindow *parent, QString const &local_dir = QString());
	~AddRepositoryDialog() override;

	QString repositoryName() const;
	QString localPath(bool cook) const;
	QString remoteName() const;
	QString remoteURL() const;
	QString overridedSshKey() const;
	AddRepositoryDialog::Mode mode() const;
	Git::CloneData makeCloneData() const;
	RepositoryData makeRepositoryData() const;
private slots:
	void on_comboBox_local_working_folder_currentTextChanged(const QString &arg1);
	void on_comboBox_search_currentIndexChanged(int index);
	void on_groupBox_remote_clicked();
	void on_groupBox_remote_toggled(bool arg1);
	void on_lineEdit_local_path_textChanged(QString const &arg1);
	void on_lineEdit_remote_repository_url_textChanged(const QString &);
	void on_pushButton_browse_local_path_clicked();
	void on_pushButton_prev_clicked();
	void on_pushButton_test_repo_clicked();
	void on_radioButton_add_existing_clicked();
	void on_radioButton_clone_clicked();
	void on_radioButton_initialize_clicked();
	void on_pushButton_manage_local_fonder_clicked();
	
public slots:
	void accept() override;
};

#endif // ADDREPOSITORYDIALOG_H
