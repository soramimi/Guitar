#include "MySettings.h"
#include "SelectItemDialog.h"
#include "ApplicationGlobal.h"
#include "SettingGeneralForm.h"
#include "ui_SettingGeneralForm.h"
#include "Languages.h"
#include "common/misc.h"

#include <QFileDialog>

SettingGeneralForm::SettingGeneralForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingGeneralForm)
{
	ui->setupUi(this);

	langs = languages();

	themes.push_back(Languages::Item("standard", tr("Standard")));
	themes.push_back(Languages::Item("dark", tr("Dark")));

	updateLanguage();
	updateTheme();
}

SettingGeneralForm::~SettingGeneralForm()
{
	delete ui;
}

QList<Languages::Item> SettingGeneralForm::languages()
{
	return Languages().items;
}

void SettingGeneralForm::exchange(bool save)
{
	if (save) {
		settings()->remember_and_restore_window_position = ui->checkBox_save_window_pos->isChecked();
		settings()->enable_high_dpi_scaling = ui->checkBox_enable_high_dpi_scaling->isChecked();
	} else {
		ui->checkBox_save_window_pos->setChecked(settings()->remember_and_restore_window_position);
		ui->checkBox_enable_high_dpi_scaling->setChecked(settings()->enable_high_dpi_scaling);
	}
}

void SettingGeneralForm::on_pushButton_browse_default_working_dir_clicked()
{
}

void SettingGeneralForm::updateLanguage()
{
	QString id = global->language_id;
	if (id.isEmpty()) {
		id = "en";
	}
	for (Languages::Item const &item : langs) {
		if (item.id == id) {
			ui->lineEdit_language->setText(item.description);
			return;
		}
	}
}

void SettingGeneralForm::updateTheme()
{
	QString id = global->theme_id;
	if (id.isEmpty()) {
		id = "standard";
	}
	for (Languages::Item const &item : themes) {
		if (item.id == id) {
			ui->lineEdit_theme->setText(item.description);
			return;
		}
	}
}

void SettingGeneralForm::execSelectLanguageDialog(QWidget *parent, QList<Languages::Item> const &langs, std::function<void()> const &done)
{
	SelectItemDialog dlg(parent);
	dlg.setWindowTitle(tr("Select Language"));
	for (Languages::Item const &item : langs) {
		dlg.addItem(item.id, item.description);
	}
	dlg.select(global->language_id.isEmpty() ? "en" : global->language_id);
	if (dlg.exec() == QDialog::Accepted) {
		Languages::Item item = dlg.item();
		global->language_id = item.id;
		MySettings s;
		s.beginGroup("UI");
		s.setValue("Language", global->language_id);
		s.endGroup();
		done();
	}
}

void SettingGeneralForm::on_pushButton_change_language_clicked()
{
	execSelectLanguageDialog(this, langs, [&](){updateLanguage();});
}

void SettingGeneralForm::on_pushButton_change_theme_clicked()
{
	SelectItemDialog dlg(this);
	dlg.setWindowTitle(tr("Select Theme"));
	for (Languages::Item const &item : themes) {
		dlg.addItem(item.id, item.description);
	}
	dlg.select(global->theme_id.isEmpty() ? "standard" : global->theme_id);
	if (dlg.exec() == QDialog::Accepted) {
		Languages::Item item = dlg.item();
		global->theme_id = item.id;
		MySettings s;
		s.beginGroup("UI");
		s.setValue("Theme", global->theme_id);
		s.endGroup();
		updateTheme();
	}
}
