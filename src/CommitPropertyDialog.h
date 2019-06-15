#ifndef COMMITPROPERTYDIALOG_H
#define COMMITPROPERTYDIALOG_H

#include <QDialog>

#include "Git.h"

class QLabel;

namespace Ui {
class CommitPropertyDialog;
}

class BasicMainWindow;

class CommitPropertyDialog : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit CommitPropertyDialog(QWidget *parent, BasicMainWindow *mw, Git::CommitItem const *commit);
	explicit CommitPropertyDialog(QWidget *parent, BasicMainWindow *mw, QString const &commit_id);
	~CommitPropertyDialog() override;

	void showCheckoutButton(bool f);
	void showJumpButton(bool f);
private slots:
	void on_pushButton_checkout_clicked();
	void on_pushButton_details_clicked();
	void on_pushButton_explorer_clicked();
	void on_pushButton_jump_clicked();
private:
	Ui::CommitPropertyDialog *ui;
	void init(BasicMainWindow *mw);
	BasicMainWindow *mainwindow();
	void setAvatar(const QIcon &icon, QLabel *label);
	void updateAvatar(bool request);
};

#endif // COMMITPROPERTYDIALOG_H
