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
private:
	Ui::SelectItemDialog *ui;
public:
	struct Item {
		QString id;
		QString text;
		Item() = default;
		Item(QString const &id, QString const &text)
			: id(id)
			, text(text)
		{
		}
	};
public:
	explicit SelectItemDialog(QWidget *parent = nullptr);
	~SelectItemDialog() override;

	void addItem(QString const &item, QString const &text);

	Item item() const;
	void select(QString const &id);
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *);
};

#endif // SELECTITEMDIALOG_H
