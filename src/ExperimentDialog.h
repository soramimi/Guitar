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

private:
	Ui::ExperimentDialog *ui;
};

#endif // EXPERIMENTDIALOG_H
