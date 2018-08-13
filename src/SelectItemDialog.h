#ifndef SELECTITEMDIALOG_H
#define SELECTITEMDIALOG_H

#include <QDialog>
#include <vector>

class QListWidgetItem;

namespace Ui {
class SelectItemDialog;
}

class SelectItemDialog : public QDialog {
	Q_OBJECT
public:
	struct Item {
		QString id;
		QString text;
		Item()
		{
		}
		Item(QString const &id, QString const &text)
			: id(id)
			, text(text)
		{
		}
	};
public:
	explicit SelectItemDialog(QWidget *parent = 0);
	~SelectItemDialog();

	void addItem(QString const &item, QString const &text);

	Item item() const;
	void select(const QString &id);
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

private:
	Ui::SelectItemDialog *ui;
};

#endif // SELECTITEMDIALOG_H
