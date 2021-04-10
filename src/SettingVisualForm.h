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

private:
	Ui::SettingVisualForm *ui;
};

#endif // SETTINGVISUALFORM_H
