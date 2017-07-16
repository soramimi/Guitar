#ifndef SETTINGDIRECTORIESFORM_H
#define SETTINGDIRECTORIESFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingDirectoriesForm;
}

class SettingDirectoriesForm : public AbstractSettingForm
{
	Q_OBJECT

public:
	explicit SettingDirectoriesForm(QWidget *parent = 0);
	~SettingDirectoriesForm();
	void exchange(bool save);
private slots:
	void on_pushButton_select_git_command_clicked();
	void on_pushButton_select_file_command_clicked();
	void on_pushButton_browse_default_working_dir_clicked();
private:
	Ui::SettingDirectoriesForm *ui;
};

#endif // SETTINGDIRECTORIESFORM_H
