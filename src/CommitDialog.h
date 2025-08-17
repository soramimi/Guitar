#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include "GenerateCommitMessageThread.h"
#include "Git.h"
#include "gpg.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class CommitDialog;
}

class CommitDialog : public QDialog {
	Q_OBJECT
public:
	explicit CommitDialog(MainWindow *parent, QString const &reponame, GitUser const &user, gpg::Data const &key, QString const &previousMessage);
	~CommitDialog() override;

	void setText(QString const &text);
	QString text() const;
	bool isSigningEnabled() const;
	bool isAmend() const;
protected:
	void keyPressEvent(QKeyEvent *event) override;
private:
	Ui::CommitDialog *ui;
	gpg::Data key_;
	QString previousMessage_;
	std::string diff_;
	GenerateCommitMessageThread generator_;
	std::string commit_message_;

	MainWindow *mainwindow();
	void updateSigningInfo();
	void updateUI(bool enable);
private slots:
	void on_pushButton_config_signing_clicked();
	void on_checkbox_amend_stateChanged(int state);
	void on_pushButton_generate_with_ai_clicked();
	void onReady(GeneratedCommitMessage const &message);
	void on_plainTextEdit_textChanged();
};

#endif // COMMITDIALOG_H
