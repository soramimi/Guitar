#ifndef SELECTITEMDIALOG_H
#define SELECTITEMDIALOG_H

#include <QDialog>
#include <vector>
#include "Languages.h"

class QListWidgetItem;

namespace Ui {
class SelectItemDialog;
}

class SelectItemDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SelectItemDialog *ui;
public:
	explicit SelectItemDialog(QWidget *parent = nullptr);
	~SelectItemDialog() override;

	void addItem(QString const &id, QString const &text);

	Languages::Item item() const;
	void select(QString const &id);
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *);
};

#endif // SELECTITEMDIALOG_H
