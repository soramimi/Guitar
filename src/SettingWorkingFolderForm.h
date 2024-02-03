#ifndef SETTINGWORKINGFOLDERFORM_H
#define SETTINGWORKINGFOLDERFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingWorkingFolderForm;
}

class SettingWorkingFolderForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingWorkingFolderForm *ui;
public:
	explicit SettingWorkingFolderForm(QWidget *parent = nullptr);
	~SettingWorkingFolderForm() override;
	void exchange(bool save) override;
private slots:
	void on_pushButton_clicked();
	void on_pushButton_add_clicked();
	void on_pushButton_remove_clicked();
};

#endif // SETTINGWORKINGFOLDERFORM_H
