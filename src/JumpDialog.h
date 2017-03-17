#ifndef JUMPDIALOG_H
#define JUMPDIALOG_H

#include "MyTableWidgetDelegate.h"
#include "Git.h"
#include <QDialog>

namespace Ui {
class JumpDialog;
}

class QTableWidgetItem;

class JumpDialog : public QDialog
{
	Q_OBJECT
public:
private:
	MyTableWidgetDelegate delegate_;
	QString filter_text;
	QString selected_name;
	QList<Git::NamedCommitItem> list;
public:
	explicit JumpDialog(QWidget *parent, QList<Git::NamedCommitItem> const &list);
	~JumpDialog();

	QString selectedName() const;

	static void sort(QList<Git::NamedCommitItem> *items);
private slots:
	void on_toolButton_clicked();
	void on_lineEdit_filter_textChanged(const QString &text);
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

private:
	Ui::JumpDialog *ui;
	void updateTable();
	void updateTable_(const QList<Git::NamedCommitItem> &list2);
};

#endif // JUMPDIALOG_H
