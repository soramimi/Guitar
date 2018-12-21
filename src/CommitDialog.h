#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class CommitDialog;
}

class CommitDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CommitDialog(MainWindow *parent, QString const &reponame, Git::User const &user, gpg::Data const &key);
	~CommitDialog();

	void setText(QString const &text);
	QString text() const;
	bool isSigningEnabled() const;
protected:
	void keyPressEvent(QKeyEvent *event);
private:
	Ui::CommitDialog *ui;
	gpg::Data key_;

	// QDialog interface
	MainWindow *mainwindow();
	void updateSigningInfo();
private slots:
	void on_pushButton_config_signing_clicked();
};

#endif // COMMITDIALOG_H
