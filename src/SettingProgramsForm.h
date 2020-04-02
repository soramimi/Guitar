#ifndef SETTINGPROGRAMSFORM_H
#define SETTINGPROGRAMSFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingProgramsForm;
}

class SettingProgramsForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingProgramsForm *ui;
public:
	explicit SettingProgramsForm(QWidget *parent = nullptr);
	~SettingProgramsForm() override;
	void exchange(bool save) override;
private slots:
	void on_pushButton_select_git_command_clicked();
	void on_pushButton_select_file_command_clicked();
	void on_pushButton_select_gpg_command_clicked();

	void on_pushButton_select_ssh_command_clicked();
};

#endif // SETTINGPROGRAMSFORM_H
