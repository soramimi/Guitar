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

	auto AddPage = [&](QString const &name, QWidget *page){
		item = new QTreeWidgetItem();
		item->setText(0, name);
		item->setData(0, Qt::UserRole, QVariant((uintptr_t)(QWidget *)page));
		ui->treeWidget->addTopLevelItem(item);
	};
	AddPage(tr("Directories"), ui->page_directories);
	AddPage(tr("Network"), ui->page_network);
	AddPage(tr("Example"), ui->page_example);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::loadSettings(ApplicationSettings *as)
{
	MySettings s;

	s.beginGroup("Global");
	as->default_working_dir = s.value("DefaultWorkingDirectory").toString();
	as->git_command = s.value("GitCommand").toString();
	as->file_command = s.value("FileCommand").toString();
	s.endGroup();

	s.beginGroup("Network");
	as->proxy_type = s.value("ProxyType").toString();
	as->proxy_server = misc::makeProxyServerURL(s.value("ProxyServer").toString());
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

	s.beginGroup("Network");
	s.setValue("ProxyType", set.proxy_type);
	s.setValue("ProxyServer", misc::makeProxyServerURL(set.proxy_server));
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

