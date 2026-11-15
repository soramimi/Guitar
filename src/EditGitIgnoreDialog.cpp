#include "EditGitIgnoreDialog.h"
#include "ui_EditGitIgnoreDialog.h"
#include "MainWindow.h"
#include "TextEditDialog.h"
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <array>

namespace {
bool isOutsideDirectory(QString const &relative)
{
	return relative == ".." || relative.startsWith("../") || QDir::isAbsolutePath(relative);
}

QString escapeIgnorePattern(QString const &path)
{
	QString pattern;
	for (QChar c : path) {
		if (c == '\\' || c == '*' || c == '?' || c == '[' || c == ']' || c == ' ') {
			pattern += '\\';
		}
		pattern += c;
	}
	return pattern;
}
}

EditGitIgnoreDialog::EditGitIgnoreDialog(MainWindow *parent, QString const &file)
	: QDialog(parent)
	, ui(new Ui::EditGitIgnoreDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	working_dir_ = QDir(parent->currentWorkingCopyDir()).absolutePath();
	file_ = QDir::fromNativeSeparators(file);
	
	updateComboBox();
	updateRadioButtons();
}

EditGitIgnoreDialog::~EditGitIgnoreDialog()
{
	delete ui;
}

void EditGitIgnoreDialog::updateComboBox()
{
	QDir workdir(working_dir_);
	// Keep repository paths lexical: resolving symlinks can leave the worktree,
	// and the selected file may already have been deleted.
	QString dir = QFileInfo(workdir.absoluteFilePath(file_)).absolutePath();
	QStringList gitignores;
	while (dir != working_dir_) {
		if (isOutsideDirectory(workdir.relativeFilePath(dir))) break;
		QString gitignore = QDir(dir).filePath(".gitignore");
		if (QFileInfo(gitignore).isFile()) {
			gitignores.prepend(gitignore);
		}
		dir = QDir::cleanPath(dir + "/..");
	}
	// The root file is also the creation target when no .gitignore exists yet.
	gitignores.prepend(workdir.filePath(".gitignore"));
	QSignalBlocker blocker(ui->comboBox_file);
	ui->comboBox_file->clear();
	for (QString const &gitignore : gitignores) {
		ui->comboBox_file->addItem(gitignore);
	}
}

void EditGitIgnoreDialog::updateRadioButtons()
{
	QDir destdir(QFileInfo(gitignorePath()).absolutePath());
	text_map_.clear();
	
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
		bool directory = path.endsWith('/');
		path = destdir.relativeFilePath(QDir(working_dir_).absoluteFilePath(path));
		if (path != "." && !isOutsideDirectory(path)) {
			// Anchor individual paths; a bare basename also matches descendants.
			SetText(button, "/" + escapeIgnorePattern(path) + (directory ? "/" : ""));
		}
	};
	
	SetPath(ui->radioButton_1, file_);

	if (!info.suffix().isEmpty()) {
		SetText(ui->radioButton_2, "*." + escapeIgnorePattern(info.suffix()));
	}

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

void EditGitIgnoreDialog::on_comboBox_file_currentTextChanged(const QString &)
{
	updateRadioButtons();
}

