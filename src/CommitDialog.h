#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

class BasicMainWindow;

namespace Ui {
class CommitDialog;
}

class CommitDialog : public QDialog {
	Q_OBJECT
public:
	explicit CommitDialog(BasicMainWindow *parent, QString const &reponame, Git::User const &user, gpg::Data const &key);
	~CommitDialog() override;

	void setText(QString const &text);
	QString text() const;
	bool isSigningEnabled() const;
protected:
	void keyPressEvent(QKeyEvent *event) override;
private:
	Ui::CommitDialog *ui;
	gpg::Data key_;

	// QDialog interface
	BasicMainWindow *mainwindow();
	void updateSigningInfo();
private slots:
	void on_pushButton_config_signing_clicked();
};

#endif // COMMITDIALOG_H
