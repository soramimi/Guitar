#include "SettingVisualForm.h"
#include "ui_SettingVisualForm.h"
#include "MySettings.h"
// #include "ApplicationGlobal.h"

SettingVisualForm::SettingVisualForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingVisualForm)
{
	ui->setupUi(this);
}

SettingVisualForm::~SettingVisualForm()
{
	delete ui;
}

void SettingVisualForm::exchange(bool save)
{
	if (save) {
		settings()->branch_label_color.head = ui->toolButton_head->color();
		settings()->branch_label_color.local = ui->toolButton_local_branch->color();
		settings()->branch_label_color.remote = ui->toolButton_remote_branch->color();
		settings()->branch_label_color.tag = ui->toolButton_tag->color();

		settings()->fixed_font.custom = ui->checkBox_fixed_font->isChecked();
		settings()->fixed_font.font_name = ui->fontComboBox_fixed_font->currentText();
		settings()->fixed_font.size = ui->spinBox_fixed_font_size->value();
	} else {
		ui->toolButton_head->setColor(settings()->branch_label_color.head);
		ui->toolButton_local_branch->setColor(settings()->branch_label_color.local);
		ui->toolButton_remote_branch->setColor(settings()->branch_label_color.remote);
		ui->toolButton_tag->setColor(settings()->branch_label_color.tag);

		ui->checkBox_fixed_font->setChecked(settings()->fixed_font.custom);
		ui->fontComboBox_fixed_font->setCurrentFont(QFont(settings()->fixed_font.font_name));
		ui->spinBox_fixed_font_size->setValue(settings()->fixed_font.size);
		ui->frame_fixed_font->setEnabled(settings()->fixed_font.custom);
	}
}

void SettingVisualForm::on_pushButton_reset_clicked()
{
	auto def = ApplicationSettings::defaultSettings();
	ui->toolButton_head->setColor(def.branch_label_color.head);
	ui->toolButton_local_branch->setColor(def.branch_label_color.local);
	ui->toolButton_remote_branch->setColor(def.branch_label_color.remote);
	ui->toolButton_tag->setColor(def.branch_label_color.tag);
}

void SettingVisualForm::on_checkBox_fixed_font_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->fontComboBox_fixed_font->setEnabled(arg1 == Qt::Checked);
}

