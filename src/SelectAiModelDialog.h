#ifndef SELECTAIMODELDIALOG_H
#define SELECTAIMODELDIALOG_H

#include <QDialog>

namespace Ui {
class SelectAiModelDialog;
}

class SelectAiModelDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit SelectAiModelDialog(QWidget *parent = nullptr);
	~SelectAiModelDialog();
	
private slots:
	void on_pushButton_load_preset_clicked();
	
	void on_pushButton_fetch_model_clicked();
	
private:
	Ui::SelectAiModelDialog *ui;
};

#endif // SELECTAIMODELDIALOG_H
