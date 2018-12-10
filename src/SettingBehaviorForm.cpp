#include "ConfigSigningDialog.h"
#include "SettingBehaviorForm.h"
#include "ui_SettingBehaviorForm.h"
#include "common/misc.h"
#include <QFileDialog>

SettingBehaviorForm::SettingBehaviorForm(QWidget *parent) :
	AbstractSettingForm(parent),
	ui(new Ui::SettingBehaviorForm)
{
	ui->setupUi(this);
}

SettingBehaviorForm::~SettingBehaviorForm()
{
	delete ui;
}

unsigned int SettingBehaviorForm::getWatchRemoteChangesEveryMins()
{
	int i = ui->comboBox_watch_remote_changes->currentIndex();
	return ui->comboBox_watch_remote_changes->itemData(i).toUInt();
}

void SettingBehaviorForm::setWatchRemoteChangesEveryMins(unsigned int min)
{
	std::vector<unsigned int> vec = { 0, 1, 3, 5, 10, 15, 30, 45, 60 };
	vec.push_back(min);
	std::sort(vec.begin(), vec.end());
	auto it = std::unique(vec.begin(), vec.end());
	vec.resize((size_t)(it - vec.begin()));

	ui->comboBox_watch_remote_changes->clear();
	for (unsigned int i = 0; i < vec.size(); i++) {
		unsigned int m = vec[i];
		QString text;
		if (m == 0) {
			text = tr("Disable");
		} else if (m == 1) {
			text = tr("1 min");
		} else {
			text = tr("%1 mins").arg(m);
		}
		ui->comboBox_watch_remote_changes->addItem(text, QVariant(m));
		if (m == min) {
			ui->comboBox_watch_remote_changes->setCurrentIndex((int)i);
		}
	}
}

void SettingBehaviorForm::exchange(bool save)
{
	if (save) {
		settings()->automatically_fetch_when_opening_the_repository = ui->checkBox_auto_fetch->isChecked();
		settings()->get_committer_icon = ui->checkBox_get_committer_icon->isChecked();
		settings()->watch_remote_changes_every_mins = getWatchRemoteChangesEveryMins();
		settings()->maximum_number_of_commit_item_acquisitions = (unsigned int)ui->spinBox_max_commit_item_acquisitions->value();
		settings()->default_working_dir = ui->lineEdit_default_working_dir->text();
	} else {
		ui->checkBox_auto_fetch->setChecked(settings()->automatically_fetch_when_opening_the_repository);
		ui->checkBox_get_committer_icon->setChecked(settings()->get_committer_icon);
		setWatchRemoteChangesEveryMins(settings()->watch_remote_changes_every_mins);
		ui->spinBox_max_commit_item_acquisitions->setValue((int)settings()->maximum_number_of_commit_item_acquisitions);
		ui->lineEdit_default_working_dir->setText(settings()->default_working_dir);
	}
}

void SettingBehaviorForm::on_pushButton_signing_policy_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), false);
	dlg.exec();
}

void SettingBehaviorForm::on_pushButton_browse_default_working_dir_clicked()
{
	QString dir = ui->lineEdit_default_working_dir->text();
	dir = QFileDialog::getExistingDirectory(this, tr("Default working folder"), dir);
	dir = misc::normalizePathSeparator(dir);
	if (QFileInfo(dir).isDir()) {
		settings()->default_working_dir = dir;
		ui->lineEdit_default_working_dir->setText(dir);
	}
}
