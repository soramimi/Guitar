#ifndef SETTINGOPTIONSFORM_H
#define SETTINGOPTIONSFORM_H

#include "AbstractSettingForm.h"
#include <QWidget>

namespace Ui {
class SettingOptionsForm;
}

class SettingOptionsForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingOptionsForm *ui;
public:
	explicit SettingOptionsForm(QWidget *parent = nullptr);
	~SettingOptionsForm();
	void exchange(bool save) override;
private slots:
	void on_pushButton_edit_profiles_clicked();
};

#endif // SETTINGOPTIONSFORM_H
