#ifndef SEARCHFROMGITHUBDIALOG_H
#define SEARCHFROMGITHUBDIALOG_H

#include "MyTableWidgetDelegate.h"

#include <QDialog>

namespace Ui {
class SearchFromGitHubDialog;
}

class QTableWidgetItem;

class SearchFromGitHubDialog : public QDialog
{
	Q_OBJECT
private:
	struct Item {
		std::string full_name;
		std::string description;
		std::string ssh_url;
		std::string clone_url;
		std::string html_url;
		double score = 0;
	};
	std::vector<Item> items;
	QString url_;
	MyTableWidgetDelegate item_delegate;
public:
	explicit SearchFromGitHubDialog(QWidget *parent = 0);
	~SearchFromGitHubDialog();

	QString url() const;

private slots:
	void on_pushButton_search_clicked();
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_radioButton_ssh_clicked();
	void on_radioButton_http_clicked();
	void onHyperlinkClicked();
private:
	Ui::SearchFromGitHubDialog *ui;
	void updateUI();
};

#endif // SEARCHFROMGITHUBDIALOG_H
