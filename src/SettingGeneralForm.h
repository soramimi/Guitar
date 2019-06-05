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
	Ui::SettingGeneralForm *ui;
	QList<SelectItemDialog::Item> langs;
	QList<SelectItemDialog::Item> themes;

	void updateLanguage();
	void updateTheme();
public:
	explicit SettingGeneralForm(QWidget *parent = nullptr);
	~SettingGeneralForm() override;

	void exchange(bool save) override;
	static QList<SelectItemDialog::Item> languages();
	static void execSelectLanguageDialog(QWidget *parent, const QList<SelectItemDialog::Item> &langs, const std::function<void ()> &done);
private slots:
	void on_pushButton_browse_default_working_dir_clicked();
	void on_pushButton_change_language_clicked();
	void on_pushButton_change_theme_clicked();
};

#endif // SETTINGGENERALFORM_H
