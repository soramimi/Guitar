#ifndef CLONEFROMGITHUBDIALOG_H
#define CLONEFROMGITHUBDIALOG_H

#include <QDialog>

namespace Ui {
class CloneFromGitHubDialog;
}

class CloneFromGitHubDialog : public QDialog
{
	Q_OBJECT
private:
	Ui::CloneFromGitHubDialog *ui;
	QString url_;
	void updateUI();
public:
	explicit CloneFromGitHubDialog(QWidget *parent, QString const &username, QString const &reponame);
	~CloneFromGitHubDialog() override;
	QString url() const;
private slots:
	void onHyperlinkClicked();
	void on_radioButton_ssh_clicked();
	void on_radioButton_http_clicked();
};

#endif // CLONEFROMGITHUBDIALOG_H
