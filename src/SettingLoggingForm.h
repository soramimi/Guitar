#ifndef SETTINGLOGGINGFORM_H
#define SETTINGLOGGINGFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingLoggingForm;
}

class SettingLoggingForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingLoggingForm *ui;
	void updateUI();
public:
	explicit SettingLoggingForm(QWidget *parent = nullptr);
	~SettingLoggingForm() override;
	void exchange(bool save) override;
private slots:
	void on_radioButton_dir_default_clicked();
	void on_radioButton_dir_custom_clicked();
	void on_pushButton_browse_output_dir_clicked();
};

#endif // SETTINGLOGGINGFORM_H
