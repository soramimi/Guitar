#ifndef SELECTCOMMANDDIALOG_H
#define SELECTCOMMANDDIALOG_H

#include <QDialog>

namespace Ui {
class SelectCommandDialog;
}

class QListWidgetItem;

class SelectCommandDialog : public QDialog
{
	Q_OBJECT
private:
	QString path;
	QString command_name;
	QStringList command_files;
public:
	explicit SelectCommandDialog(QWidget *parent, QString const &cmdname, QStringList const &cmdfiles, QString const &path, QStringList const &list);
	~SelectCommandDialog();

	QString selectedFile() const;
private slots:

	void on_listWidget_currentTextChanged(QString const &currentText);

	void on_pushButton_browse_clicked();

	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

private:
	Ui::SelectCommandDialog *ui;
};

#endif // SELECTCOMMANDDIALOG_H
