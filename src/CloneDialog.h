#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>
#include <QThread>
#include "Git.h"

namespace Ui {
class CloneDialog;
}

class MainWindow;

class CloneDialog : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *m;

	typedef std::shared_ptr<Git> GitPtr;
public:
	explicit CloneDialog(QWidget *parent, QString const &url, QString const &defworkdir);
	~CloneDialog();

	QString url();
	QString dir();
private:
	Ui::CloneDialog *ui;

	MainWindow *mainwindow();
private slots:
	void on_lineEdit_repo_location_textChanged(const QString &arg1);
	void on_pushButton_test_clicked();
	void on_comboBox_currentIndexChanged(int index);
};

#endif // CLONEDIALOG_H
