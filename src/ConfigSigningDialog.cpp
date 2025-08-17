#include "ConfigSigningDialog.h"
#include "MainWindow.h"
#include "ui_ConfigSigningDialog.h"

ConfigSigningDialog::ConfigSigningDialog(QWidget *parent, MainWindow *mw, bool local_enable)
	: QDialog(parent)
	, ui(new Ui::ConfigSigningDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
	mainwindow_ = mw;

	if (!mainwindow()->git().isValidWorkingCopy()) {
		local_enable = false;
	}
	ui->label_local->setVisible(local_enable);
	ui->comboBox_sign_local->setVisible(local_enable);

	updateSigningInfo();
}

ConfigSigningDialog::~ConfigSigningDialog()
{
	delete ui;
}

MainWindow *ConfigSigningDialog::mainwindow()
{
	return mainwindow_;
}

void ConfigSigningDialog::updateSigningInfo()
{
	GitRunner g = mainwindow()->git();

	auto InitComboBox = [](QComboBox *cb, GitSignPolicy pol){
		cb->addItem("unset");
		cb->addItem("false");
		cb->addItem("true");
		QString t;
		if (pol == GitSignPolicy::Unset) {
			t = "unset";
		} else if (pol == GitSignPolicy::False) {
			t = "false";
		} else if (pol == GitSignPolicy::True) {
			t = "true";
		}
		cb->setCurrentText(t);
	};
	gpol_ = g.signPolicy(GitSource::Global);
	lpol_ = g.signPolicy(GitSource::Local);
	InitComboBox(ui->comboBox_sign_global, gpol_);
	InitComboBox(ui->comboBox_sign_local, lpol_);

}

void ConfigSigningDialog::accept()
{
	GitRunner g = mainwindow()->git();
	auto SetSignPolicy = [&](QComboBox *cb, GitSource src, GitSignPolicy oldpol){
		GitSignPolicy pol = GitSignPolicy::Unset;
		QString s = cb->currentText();
		if (s == "false") {
			pol = GitSignPolicy::False;
		} else if (s == "true") {
			pol = GitSignPolicy::True;
		}
		if (pol != oldpol) g.setSignPolicy(src, pol);
	};
	SetSignPolicy(ui->comboBox_sign_global, GitSource::Global, gpol_);
	SetSignPolicy(ui->comboBox_sign_local, GitSource::Local, lpol_);

	QDialog::accept();
}
