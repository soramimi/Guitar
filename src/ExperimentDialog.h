#ifndef EXPERIMENTDIALOG_H
#define EXPERIMENTDIALOG_H

#include <QDialog>

namespace Ui {
class ExperimentDialog;
}

class ExperimentDialog : public QDialog {
	Q_OBJECT
public:
	explicit ExperimentDialog(QWidget *parent = nullptr);
	~ExperimentDialog() override;
private:
	Ui::ExperimentDialog *ui;
};

#endif // EXPERIMENTDIALOG_H
