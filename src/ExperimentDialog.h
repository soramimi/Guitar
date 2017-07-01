#ifndef EXPERIMENTDIALOG_H
#define EXPERIMENTDIALOG_H

#include <QDialog>

namespace Ui {
class ExperimentDialog;
}

class ExperimentDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ExperimentDialog(QWidget *parent = 0);
	~ExperimentDialog();

private slots:
	void on_pushButton_get_clicked();

private:
	Ui::ExperimentDialog *ui;
};

#endif // EXPERIMENTDIALOG_H
