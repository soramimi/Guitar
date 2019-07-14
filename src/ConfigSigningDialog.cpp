#include "ConfigSigningDialog.h"
#include "BasicMainWindow.h"
#include "ui_ConfigSigningDialog.h"

ConfigSigningDialog::ConfigSigningDialog(QWidget *parent, BasicMainWindow *mw, bool local_enable)
	: QDialog(parent)
	, ui(new Ui::ConfigSigningDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
	mainwindow_ = mw;

	if (!mainwindow()->git()->isValidWorkingCopy()) {
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

BasicMainWindow *ConfigSigningDialog::mainwindow()
{
	return mainwindow_;
}

void ConfigSigningDialog::updateSigningInfo()
{
	GitPtr g = mainwindow()->git();

	auto InitComboBox = [](QComboBox *cb, Git::SignPolicy pol){
		cb->addItem("unset");
		cb->addItem("false");
		cb->addItem("true");
		QString t;
		if (pol == Git::SignPolicy::Unset) {
			t = "unset";
		} else if (pol == Git::SignPolicy::False) {
			t = "false";
		} else if (pol == Git::SignPolicy::True) {
			t = "true";
		}
		cb->setCurrentText(t);
	};
	gpol_ = g->signPolicy(Git::Source::Global);
	lpol_ = g->signPolicy(Git::Source::Local);
	InitComboBox(ui->comboBox_sign_global, gpol_);
	InitComboBox(ui->comboBox_sign_local, lpol_);

}

void ConfigSigningDialog::accept()
{
	GitPtr g = mainwindow()->git();
	auto SetSignPolicy = [&](QComboBox *cb, Git::Source src, Git::SignPolicy oldpol){
		Git::SignPolicy pol = Git::SignPolicy::Unset;
		QString s = cb->currentText();
		if (s == "false") {
			pol = Git::SignPolicy::False;
		} else if (s == "true") {
			pol = Git::SignPolicy::True;
		}
		if (pol != oldpol) g->setSignPolicy(src, pol);
	};
	SetSignPolicy(ui->comboBox_sign_global, Git::Source::Global, gpol_);
	SetSignPolicy(ui->comboBox_sign_local, Git::Source::Local, lpol_);

	QDialog::accept();
}
