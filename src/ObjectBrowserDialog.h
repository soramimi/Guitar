#ifndef OBJECTBROWSERDIALOG_H
#define OBJECTBROWSERDIALOG_H

#include <QDialog>

class QListWidgetItem;

namespace Ui {
class ObjectBrowserDialog;
}

class ObjectBrowserDialog : public QDialog {
	Q_OBJECT
public:
	explicit ObjectBrowserDialog(QWidget *parent, QStringList const &list);
	~ObjectBrowserDialog();
	QString text() const;
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

private:
	Ui::ObjectBrowserDialog *ui;
};

#endif // OBJECTBROWSERDIALOG_H
