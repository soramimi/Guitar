#ifndef SETTINGPROGRAMSFORM_H
#define SETTINGPROGRAMSFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingProgramsForm;
}

class SettingProgramsForm : public AbstractSettingForm
{
	Q_OBJECT

public:
	explicit SettingProgramsForm(QWidget *parent = 0);
	~SettingProgramsForm();
	void exchange(bool save);
private slots:
	void on_pushButton_select_git_command_clicked();
	void on_pushButton_select_file_command_clicked();
	void on_pushButton_select_gpg_command_clicked();

private:
	Ui::SettingProgramsForm *ui;
};

#endif // SETTINGPROGRAMSFORM_H
