#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "MySettings.h"

#include <QFileDialog>
#include "misc.h"

SettingsDialog::SettingsDialog(MainWindow *parent) :
	QDialog(parent),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow_ = parent;

	loadSettings();

	QTreeWidgetItem *item;

	item = new QTreeWidgetItem();
	item->setText(0, tr("Directories"));
	item->setData(0, Qt::UserRole, QVariant((uintptr_t)(QWidget *)ui->page_directories));
	ui->treeWidget->addTopLevelItem(item);

	item = new QTreeWidgetItem();
	item->setText(0, tr("Example"));
	item->setData(0, Qt::UserRole, QVariant((uintptr_t)(QWidget *)ui->page_example));
	ui->treeWidget->addTopLevelItem(item);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::loadSettings(ApplicationSettings *set)
{
	MySettings s;
	s.beginGroup("Global");
	set->default_working_dir = s.value("DefaultWorkingDirectory").toString();
	set->git_command = s.value("GitCommand").toString();
	set->file_command = s.value("FileCommand").toString();
	s.endGroup();
}

void SettingsDialog::saveSettings()
{
	MySettings s;
	s.beginGroup("Global");
	s.setValue("DefaultWorkingDirectory", set.default_working_dir);
	s.setValue("GitCommand", set.git_command);
	s.setValue("FileCommand", set.file_command);
	s.endGroup();
}

void SettingsDialog::loadSettings()
{
	loadSettings(&set);

	QList<AbstractSettingForm *> forms = ui->stackedWidget->findChildren<AbstractSettingForm *>();
	for (AbstractSettingForm *form : forms) {
		form->reflect();
	}
}

void SettingsDialog::accept()
{
	saveSettings();
	done(QDialog::Accepted);
}

void SettingsDialog::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	(void)previous;
	if (current) {
		uintptr_t p = current->data(0, Qt::UserRole).value<uintptr_t>();
		QWidget *w = reinterpret_cast<QWidget *>(p);
		Q_ASSERT(w);
		ui->stackedWidget->setCurrentWidget(w);
	}
}

