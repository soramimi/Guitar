#ifndef SELECTGITCOMMANDDIALOG_H
#define SELECTGITCOMMANDDIALOG_H

#include <QDialog>

namespace Ui {
class SelectGitCommandDialog;
}

class SelectGitCommandDialog : public QDialog
{
	Q_OBJECT
private:
	QString path;
public:
	explicit SelectGitCommandDialog(QWidget *parent, QString const &path, QStringList const &list);
	~SelectGitCommandDialog();

	QString selectedFile() const;
private slots:

	void on_listWidget_currentTextChanged(const QString &currentText);

	void on_pushButton_browse_clicked();

private:
	Ui::SelectGitCommandDialog *ui;
};

#endif // SELECTGITCOMMANDDIALOG_H
