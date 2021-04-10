#ifndef SEARCHFROMGITHUBDIALOG_H
#define SEARCHFROMGITHUBDIALOG_H

#include "MyTableWidgetDelegate.h"
#include "GitHubAPI.h"

#include <QDialog>

namespace Ui {
class SearchFromGitHubDialog;
}

class QTableWidgetItem;
class MainWindow;

class SearchFromGitHubDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SearchFromGitHubDialog *ui;
	QList<GitHubAPI::SearchResultItem> items;
	QString url_;
	MyTableWidgetDelegate item_delegate;
	MainWindow *mainwindow;

	void updateUI();
public:
	explicit SearchFromGitHubDialog(QWidget *parent, MainWindow *mw);
	~SearchFromGitHubDialog() override;

	QString url() const;

private slots:
	void on_pushButton_search_clicked();
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_radioButton_ssh_clicked();
	void on_radioButton_http_clicked();
	void onHyperlinkClicked();
};

#endif // SEARCHFROMGITHUBDIALOG_H
