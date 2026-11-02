#ifndef SETTINGVISUALFORM_H
#define SETTINGVISUALFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingVisualForm;
}

class SettingVisualForm : public AbstractSettingForm {
	Q_OBJECT
public:
	explicit SettingVisualForm(QWidget *parent = nullptr);
	~SettingVisualForm();
	void exchange(bool save) override;
private slots:
	void on_pushButton_reset_clicked();

	
	void on_checkBox_fixed_font_checkStateChanged(const Qt::CheckState &arg1);
	
private:
	Ui::SettingVisualForm *ui;
	void updateUseCustomFont(bool use);
};

#endif // SETTINGVISUALFORM_H
