#ifndef SETTINGPROGRAMS2FORM_H
#define SETTINGPROGRAMS2FORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingPrograms2Form;
}

class SettingPrograms2Form : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingPrograms2Form *ui;
public:
	explicit SettingPrograms2Form(QWidget *parent = nullptr);
	~SettingPrograms2Form() override;
	void exchange(bool save) override;
private slots:
	void on_pushButton_select_terminal_command_clicked();
	void on_pushButton_select_explorer_command_clicked();
	void on_pushButton_reset_terminal_command_clicked();
};

#endif // SETTINGPROGRAMS2FORM_H
