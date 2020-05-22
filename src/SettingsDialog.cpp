#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "MySettings.h"
#include "common/misc.h"
#include <QFileDialog>

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

	auto AddPage = [&](QWidget *page){
		page->layout()->setMargin(0);
		QString name = page->windowTitle();
		item = new QTreeWidgetItem();
		item->setText(0, name);
		item->setData(0, Qt::UserRole, QVariant::fromValue((uintptr_t)(QWidget *)page));
		ui->treeWidget->addTopLevelItem(item);
	};
	AddPage(ui->page_general);
	AddPage(ui->page_programs);
	AddPage(ui->page_behavior);
	AddPage(ui->page_network);
//	AddPage(ui->page_example);

	ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(page_number));
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

namespace {

template <typename T> class GetValue {
private:
public:
	MySettings &settings;
	QString name;
	GetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
	void operator >> (T &value)
	{
		value = settings.value(name, value).template value<T>();
	}
};

template <typename T> class SetValue {
private:
public:
	MySettings &settings;
	QString name;
	SetValue(MySettings &s, QString const &name)
		: settings(s)
		, name(name)
	{
	}
	void operator << (T const &value)
	{
		settings.setValue(name, value);
	}
};

} // namespace

void SettingsDialog::loadSettings(ApplicationSettings *as)
{
	MySettings s;

	*as = ApplicationSettings::defaultSettings();

	s.beginGroup("Global");
	GetValue<bool>(s, "SaveWindowPosition")                  >> as->remember_and_restore_window_position;
	GetValue<QString>(s, "DefaultWorkingDirectory")          >> as->default_working_dir;
	GetValue<QString>(s, "GitCommand")                       >> as->git_command;
	GetValue<QString>(s, "FileCommand")                      >> as->file_command;
	GetValue<QString>(s, "GpgCommand")                       >> as->gpg_command;
	GetValue<QString>(s, "SshCommand")                       >> as->ssh_command;
	s.endGroup();

	s.beginGroup("UI");
	GetValue<bool>(s, "EnableHighDpiScaling")                >> as->enable_high_dpi_scaling;
	GetValue<bool>(s, "ShowLabels")                          >> as->show_labels;
	s.endGroup();

	s.beginGroup("Network");
	GetValue<QString>(s, "ProxyType")                        >> as->proxy_type;
	GetValue<QString>(s, "ProxyServer")                      >> as->proxy_server;
	GetValue<bool>(s, "GetCommitterIcon")                    >> as->get_committer_icon;
	s.endGroup();
	as->proxy_server = misc::makeProxyServerURL(as->proxy_server);

	s.beginGroup("Behavior");
	GetValue<bool>(s, "AutomaticFetch")                      >> as->automatically_fetch_when_opening_the_repository;
	GetValue<unsigned int>(s, "MaxCommitItemAcquisitions")   >> as->maximum_number_of_commit_item_acquisitions;
	s.endGroup();
}

void SettingsDialog::saveSettings(ApplicationSettings const *as)
{
	MySettings s;

	s.beginGroup("Global");
	SetValue<bool>(s, "SaveWindowPosition")                  << as->remember_and_restore_window_position;
	SetValue<QString>(s, "DefaultWorkingDirectory")          << as->default_working_dir;
	SetValue<QString>(s, "GitCommand")                       << as->git_command;
	SetValue<QString>(s, "FileCommand")                      << as->file_command;
	SetValue<QString>(s, "GpgCommand")                       << as->gpg_command;
	SetValue<QString>(s, "SshCommand")                       << as->ssh_command;
	s.endGroup();

	s.beginGroup("UI");
	SetValue<bool>(s, "EnableHighDpiScaling")                << as->enable_high_dpi_scaling;
	SetValue<bool>(s, "ShowLabels")                          << as->show_labels;
	s.endGroup();

	s.beginGroup("Network");
	SetValue<QString>(s, "ProxyType")                        << as->proxy_type;
	SetValue<QString>(s, "ProxyServer")                      << misc::makeProxyServerURL(as->proxy_server);
	SetValue<bool>(s, "GetCommitterIcon")                    << as->get_committer_icon;
	s.endGroup();

	s.beginGroup("Behavior");
	SetValue<bool>(s, "AutomaticFetch")                      << as->automatically_fetch_when_opening_the_repository;
	SetValue<unsigned int>(s, "MaxCommitItemAcquisitions")   << as->maximum_number_of_commit_item_acquisitions;
	s.endGroup();

}

void SettingsDialog::saveSettings()
{
	saveSettings(&set);
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
	page_number = ui->treeWidget->currentIndex().row();
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

