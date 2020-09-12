#include "SettingVisualForm.h"
#include "ui_SettingVisualForm.h"
#include "MySettings.h"
#include "ApplicationGlobal.h"

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
	} else {
		ui->toolButton_head->setColor(settings()->branch_label_color.head);
		ui->toolButton_local_branch->setColor(settings()->branch_label_color.local);
		ui->toolButton_remote_branch->setColor(settings()->branch_label_color.remote);
		ui->toolButton_tag->setColor(settings()->branch_label_color.tag);
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
