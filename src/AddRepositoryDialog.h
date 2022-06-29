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

	Mode mode_ = Clone;
	QString reponame_;
	QString already_exists_;

	MainWindow *mainwindow();
	MainWindow const *mainwindow() const;
	QString defaultWorkingDir() const;

	void validate(bool change_name);
	void setRemoteURL(const QString &url);
	void browseLocalPath();
	void updateUI();
	void complementRemoteURL(bool toggle);
public:
	explicit AddRepositoryDialog(MainWindow *parent, QString const &dir = QString());
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
	void on_groupBox_remote_toggled(bool arg1);
	void on_lineEdit_bookmark_name_textChanged(QString const &arg1);
	void on_lineEdit_local_path_textChanged(QString const &arg1);
	void on_pushButton_test_repo_clicked();
	void on_radioButton_clone_clicked();
	void on_radioButton_add_existing_clicked();
	void on_radioButton_initialize_clicked();
	void on_comboBox_search_currentIndexChanged(int index);

	void on_lineEdit_remote_url_textChanged(const QString &arg1);

	void on_pushButton_browse_local_path_clicked();


	void on_pushButton_prev_clicked();

public slots:
	void accept() override;

	// QObject interface
public:
	bool eventFilter(QObject *watched, QEvent *event);
};

#endif // ADDREPOSITORYDIALOG_H
