#ifndef SELECTAIMODELDIALOG_H
#define SELECTAIMODELDIALOG_H

#include <QDialog>

namespace Ui {
class SelectAiModelDialog;
}

class SelectAiModelDialog : public QDialog
{
	Q_OBJECT
private:
	Ui::SelectAiModelDialog *ui;
	struct Private;
	Private *m;	
	void on_cred_key_source_changed();
public:
	explicit SelectAiModelDialog(QWidget *parent = nullptr);
	~SelectAiModelDialog();
	
private slots:
	void on_pushButton_load_preset_clicked();
	void on_pushButton_fetch_model_clicked();
	void on_comboBox_api_type_currentIndexChanged(int index);
	void on_comboBox_provider_currentIndexChanged(int index);
	void on_radioButton_cred_environ_clicked();
	void on_radioButton_cred_custom_clicked();
	void on_checkBox_show_api_key_clicked();
	void on_lineEdit_cred_symbol_textChanged(const QString &arg1);
};

#endif // SELECTAIMODELDIALOG_H
