#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "MySettings.h"

#include <QFileDialog>
#include "common/misc.h"

static int page_number = 0;


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
		item->setData(0, Qt::UserRole, QVariant::fromValue((uintptr_t)(QWidget *)page));
		ui->treeWidget->addTopLevelItem(item);
	};
	AddPage(tr("Behavior"), ui->page_behavior);
	AddPage(tr("Directories"), ui->page_directories);
	AddPage(tr("Network"), ui->page_network);
	AddPage(tr("Example"), ui->page_example);

	ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(page_number));
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::loadSettings(ApplicationSettings *as)
{
	MySettings s;

	auto STRING_VALUE = [&](QString const &name, QString &v){
		v = s.value(name, v).toString();
	};

	auto BOOL_VALUE = [&](QString const &name, bool &v){
		v = s.value(name, v).toBool();
	};

	s.beginGroup("Global");
	STRING_VALUE("DefaultWorkingDirectory", as->default_working_dir);
	STRING_VALUE("GitCommand", as->git_command);
	STRING_VALUE("FileCommand", as->file_command);
	s.endGroup();

	s.beginGroup("Network");
	STRING_VALUE("ProxyType", as->proxy_type);
	STRING_VALUE("ProxyServer", as->proxy_server);
	as->proxy_server = misc::makeProxyServerURL(as->proxy_server);
	BOOL_VALUE("GetCommitterIcon", as->get_committer_icon);
	s.endGroup();

	s.beginGroup("Behavior");
	BOOL_VALUE("AutomaticFetch", as->automatically_fetch_when_opening_the_repository);
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
	s.setValue("GetCommitterIcon", set.get_committer_icon);
	s.endGroup();

	s.beginGroup("Behavior");
	s.setValue("AutomaticFetch", set.automatically_fetch_when_opening_the_repository);
	s.endGroup();
}

void SettingsDialog::exchange(bool save)
{
	QList<AbstractSettingForm *> forms = ui->stackedWidget->findChildren<AbstractSettingForm *>();
	for (AbstractSettingForm *form : forms) {
		form->exchange(save);
	}
}

void SettingsDialog::loadSettings()
{
	loadSettings(&set);
	exchange(false);
}

void SettingsDialog::done(int r)
{
	page_number = ui->stackedWidget->currentIndex();
	QDialog::done(r);
}

void SettingsDialog::accept()
{
	exchange(true);
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

