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
	static bool saveFavoliteDirs(QStringList const &favdirs);
	static bool loadFavoliteDirs(QStringList *favdirs);
private slots:
	void on_pushButton_clicked();
	void on_pushButton_add_clicked();
	void on_pushButton_remove_clicked();
	void on_pushButton_up_clicked();
	void on_pushButton_down_clicked();
};

#endif // SETTINGWORKINGFOLDERFORM_H
