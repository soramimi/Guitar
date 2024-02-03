#include "SettingWorkingFolderForm.h"
#include "ui_SettingWorkingFolderForm.h"
#include "ApplicationGlobal.h"
#include <QFileDialog>

SettingWorkingFolderForm::SettingWorkingFolderForm(QWidget *parent)
	: AbstractSettingForm(parent)
	, ui(new Ui::SettingWorkingFolderForm)
{
	ui->setupUi(this);

	ui->lineEdit_recentry_used->setText(global->appsettings.default_working_dir);

	for (const QString &dir : global->appsettings.favorite_working_dirs) {
		ui->listWidget_favorites->addItem(dir);
	}

}

SettingWorkingFolderForm::~SettingWorkingFolderForm()
{
	delete ui;
}

void SettingWorkingFolderForm::exchange(bool save)
{
	QString favoritedirs_ini = global->app_config_dir / "favoritedirs.ini";
	if (save) {
		settings()->default_working_dir = ui->lineEdit_recentry_used->text();
		{
			settings()->favorite_working_dirs.clear();
			for (int i = 0; i < ui->listWidget_favorites->count(); i++) {
				settings()->favorite_working_dirs.append(ui->listWidget_favorites->item(i)->text());
			}
			QFile file(favoritedirs_ini);
			if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
				QTextStream out(&file);
				for (QString const &dir : settings()->favorite_working_dirs) {
					out << dir << "\n";
				}
			}
		}
	} else {
		ui->lineEdit_recentry_used->setText(settings()->default_working_dir);
		{
			QStringList list;
			QFile file(favoritedirs_ini);
			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);
				while (!in.atEnd()) {
					QString line = in.readLine().trimmed();
					if (!line.isEmpty()) {
						list.append(line);
					}
				}
				global->appsettings.favorite_working_dirs = list;
				ui->listWidget_favorites->clear();
				for (const QString &s : list) {
					ui->listWidget_favorites->addItem(s);
				}
			}
		}
	}
}

void SettingWorkingFolderForm::on_pushButton_clicked()
{
	QString dir = ui->lineEdit_recentry_used->text();
	dir = QFileDialog::getExistingDirectory(this, tr("Select Working Folder"), dir);
	if (!dir.isEmpty()) {
		ui->lineEdit_recentry_used->setText(dir);
	}
}


void SettingWorkingFolderForm::on_pushButton_add_clicked()
{
	QString newitem = ui->lineEdit_recentry_used->text();
	QStringList list;
	for (int i = 0; i < ui->listWidget_favorites->count(); i++) {
		QString s = ui->listWidget_favorites->item(i)->text();
		if (s != newitem) {
			list.append(s);
		}
	}
	list.insert(0, newitem);
	ui->listWidget_favorites->clear();
	for (const QString &s : list) {
		ui->listWidget_favorites->addItem(s);
	}
}


void SettingWorkingFolderForm::on_pushButton_remove_clicked()
{
	int row = ui->listWidget_favorites->currentRow();
	if (row >= 0) {
		delete ui->listWidget_favorites->takeItem(row);
	}
}

