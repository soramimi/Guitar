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
	void on_pushButton_setup_migemo_dict_clicked();
	void on_pushButton_delete_migemo_dict_clicked();
	void on_checkBox_incremental_search_with_migemo_checkStateChanged(const Qt::CheckState &arg1);
};

#endif // SETTINGOPTIONSFORM_H
