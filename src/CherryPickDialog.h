
#ifndef CHERRYPICKDIALOG_H
#define CHERRYPICKDIALOG_H

#include <QDialog>

class QTableWidgetItem;
struct GitCommitItem;

namespace Ui {
class CherryPickDialog;
}

class CherryPickDialog : public QDialog {
	Q_OBJECT
private:
	Ui::CherryPickDialog *ui;
public:
	explicit CherryPickDialog(QWidget *parent, const GitCommitItem &head, const GitCommitItem &pick, QList<GitCommitItem> const &parents);
	~CherryPickDialog();
	int number() const;
	bool allowEmpty() const;
private slots:
	void on_tableWidget_mainline_itemDoubleClicked(QTableWidgetItem *);
};

#endif // CHERRYPICKDIALOG_H
