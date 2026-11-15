#include "EditGitIgnoreDialog.h"
#include "ui_EditGitIgnoreDialog.h"
#include "MainWindow.h"
#include "TextEditDialog.h"
#include <QFileInfo>
#include <array>
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "common/realpath.h"

EditGitIgnoreDialog::EditGitIgnoreDialog(MainWindow *parent, QString const &file)
	: QDialog(parent)
	, ui(new Ui::EditGitIgnoreDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	working_dir_ = global->mainwindow->currentWorkingCopyDir();
	working_dir_.replace('\\', '/');
	
	// gitignore_path_ = working_dir_ / ".gitignore";
	// gitignore_path_ = misc::realpath(gitignore_path_);

	file_ = file;
	file_.replace('\\', '/');
	
	// ui->comboBox_file->addItem(gitignore_path_);
	
	updateComboBox();
	updateRadioButtons();
}

EditGitIgnoreDialog::~EditGitIgnoreDialog()
{
	delete ui;
}

void EditGitIgnoreDialog::updateComboBox()
{
	std::vector<QString> gitignores;
	{
		QString workdir = working_dir_;
		workdir.replace('\\', '/');
		if (workdir.endsWith('/')) {
			workdir.chop(1);
		}
		QString path = misc::realpath(file_);
		path.replace('\\', '/');
		while (1) {
			int i = path.lastIndexOf('/');
			if (i < 0) break;
			QString dir = path.left(i);
			QString gitignore = dir / ".gitignore";
			QFileInfo info(gitignore);
			if (info.isFile()) {
				gitignores.push_back(gitignore);
			}
			if (dir == workdir) break;
			path = dir;
		}
		std::sort(gitignores.begin(), gitignores.end());
	}
	ui->comboBox_file->clear();
	for (QString const &gitignore : gitignores) {
		ui->comboBox_file->addItem(gitignore);
	}
}

void EditGitIgnoreDialog::updateRadioButtons()
{
	QString gitignore_file  = gitignorePath();
	gitignore_file.replace('\\', '/');
	
	{
		QRadioButton *buttons[] = {
			ui->radioButton_1,
			ui->radioButton_2,
			ui->radioButton_3,
			ui->radioButton_4,
		};
	
		for (QRadioButton *button : buttons) {
			button->setText({});
			button->setVisible(false);
		}
	}
	
	QFileInfo info(file_);
	
	auto SetText = [&](QRadioButton *button, QString text) {
		if (!text.isEmpty()) {
			text_map_[button] = text;
			button->setText(text);
			button->setVisible(true);
		}
	};

	auto SetPath = [&](QRadioButton *button, QString path) {
		path = working_dir_ / path;
		int i = gitignore_file.lastIndexOf('/');
		if (i > 0) {
			QString destdir = gitignore_file.left(i + 1);
			if (path.startsWith(destdir)) {
				path = path.mid(destdir.length());
				if (!path.isEmpty()) {
					SetText(button, path);
				}
			}
		}
	};
	
	SetPath(ui->radioButton_1, file_);

	SetText(ui->radioButton_2, "*." + info.suffix());

	int i = file_.indexOf('/');
	if (i > 0) {
		SetPath(ui->radioButton_3, file_.mid(0, i + 1));
		int j = file_.lastIndexOf('/');
		if (i < j) {
			SetPath(ui->radioButton_4, file_.mid(0, j + 1));
		}
	}

	ui->radioButton_1->setFocus();
	ui->radioButton_1->click();
}

QString EditGitIgnoreDialog::gitignorePath() const
{
	return ui->comboBox_file->currentText();
}

MainWindow *EditGitIgnoreDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

QString EditGitIgnoreDialog::text() const
{
	std::array<QRadioButton *, 4> arr = {ui->radioButton_1, ui->radioButton_2, ui->radioButton_3, ui->radioButton_4};
	for (auto button : arr) {
		if (button->isChecked()) {
			auto it = text_map_.find(button);
			if (it != text_map_.end()) {
				return it->second;
			}
		}
	}
	return QString();
}

void EditGitIgnoreDialog::on_pushButton_edit_file_clicked()
{
	if (TextEditDialog::editFile(this, gitignorePath(), ".gitignore", text() + '\n')) {
		mainwindow()->updateCurrentFileList();
		done(QDialog::Rejected);
	}
}

void EditGitIgnoreDialog::on_comboBox_file_currentTextChanged(const QString &arg1)
{
	updateRadioButtons();
}

