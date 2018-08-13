#ifndef SETTINGGENERALFORM_H
#define SETTINGGENERALFORM_H

#include "AbstractSettingForm.h"
#include "SelectItemDialog.h"

#include <QWidget>

namespace Ui {
class SettingGeneralForm;
}

class SettingGeneralForm : public AbstractSettingForm
{
	Q_OBJECT
private:
	QList<SelectItemDialog::Item> langs;
	QList<SelectItemDialog::Item> themes;
public:
	explicit SettingGeneralForm(QWidget *parent = 0);
	~SettingGeneralForm();

private:
	Ui::SettingGeneralForm *ui;

	void updateLanguage();
	void updateTheme();
public:
	void exchange(bool save);
private slots:
	void on_pushButton_browse_default_working_dir_clicked();
	void on_pushButton_change_language_clicked();
	void on_pushButton_change_theme_clicked();
};

#endif // SETTINGGENERALFORM_H
