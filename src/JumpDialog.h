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
	struct Private;
	Private *m;
public:
	explicit JumpDialog(QWidget *parent, NamedCommitList const &items);
	~JumpDialog();

	QString selectedName() const;

	static void sort(NamedCommitList *items);
	bool isCheckoutChecked();
private slots:
	void on_toolButton_clicked();
	void on_lineEdit_filter_textChanged(const QString &text);
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

private:
	Ui::JumpDialog *ui;
	void updateTable();
	void updateTable_(const NamedCommitList &list2);
};

#endif // JUMPDIALOG_H
