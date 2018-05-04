#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include "Git.h"
#include "gpg.h"

#include <QDialog>

namespace Ui {
class CommitDialog;
}

class CommitDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CommitDialog(QWidget *parent, const QString &reponame, Git::User const &user, gpg::Key const &key);
	~CommitDialog();

	void setText(const QString &text);
	QString text() const;
	bool isSigningEnabled() const;
protected:
	void keyPressEvent(QKeyEvent *event);
private:
	Ui::CommitDialog *ui;
};

#endif // COMMITDIALOG_H
