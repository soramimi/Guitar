#ifndef SETTINGBEHAVIORFORM_H
#define SETTINGBEHAVIORFORM_H

#include "AbstractSettingForm.h"

#include <QWidget>

namespace Ui {
class SettingBehaviorForm;
}

class SettingBehaviorForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingBehaviorForm *ui;
public:
	explicit SettingBehaviorForm(QWidget *parent = nullptr);
	~SettingBehaviorForm() override;

public:
	void exchange(bool save) override;
private slots:
	void on_pushButton_signing_policy_clicked();
	void on_pushButton_browse_default_working_dir_clicked();
};

#endif // SETTINGBEHAVIORFORM_H
