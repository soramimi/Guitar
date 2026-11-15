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

EditGitIgnoreDialog::EditGitIgnoreDialog(MainWindow *parent, const QStringList &files)
	: QDialog(parent)
	, ui(new Ui::EditGitIgnoreDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
	
	working_dir_ = QDir(parent->currentWorkingCopyDir()).absolutePath();
	
	for (QString const &file : files) {
		QString f = QDir::fromNativeSeparators(file);
		if (f.isEmpty() || QDir::isAbsolutePath(f) || f.contains('\n') || f.contains('\r') || f.contains(QChar(0))) {
			validation_error_ = tr("The selected paths cannot be represented safely in .gitignore.");
			break;
		}
		bool directory = f.endsWith('/');
		f = QDir::cleanPath(f);
		if (f == "." || isOutsideDirectory(f)) {
			validation_error_ = tr("The selected paths must be inside the repository.");
			break;
		}
		// Do not hide the ignore configuration itself, including nested files.
		if (QFileInfo(f).fileName() == ".gitignore") continue;
		if (directory) f += '/';
		if (!files_.contains(f)) files_.push_back(f);
	}
	if (validation_error_.isEmpty() && files_.isEmpty()) {
		validation_error_ = tr("No files to ignore. .gitignore files are excluded.");
	}
	
	updateComboBox();
	updateRadioButtons();
}

EditGitIgnoreDialog::~EditGitIgnoreDialog()
{
	delete ui;
}

void EditGitIgnoreDialog::updateComboBox()
{
	QSignalBlocker blocker(ui->comboBox_file);
	ui->comboBox_file->clear();
	if (!validation_error_.isEmpty()) return;
	QDir workdir(working_dir_);
	// Keep repository paths lexical: resolving symlinks can leave the worktree,
	// and the selected file may already have been deleted.
	QString dir = QFileInfo(workdir.absoluteFilePath(files_.front())).absolutePath();
	QStringList gitignores;
	while (true) {
		if (isOutsideDirectory(workdir.relativeFilePath(dir))) break;
		QDir destdir(dir);
		bool common = true;
		for (QString const &file : files_) {
			QString relative = destdir.relativeFilePath(workdir.absoluteFilePath(file));
			if (relative == "." || isOutsideDirectory(relative)) {
				common = false;
				break;
			}
		}
		QString gitignore = QDir(dir).filePath(".gitignore");
		// Keep the root as a creation target when it does not exist yet.
		if (common && (dir == working_dir_ || QFileInfo(gitignore).isFile())) {
			gitignores.prepend(gitignore);
		}
		if (dir == working_dir_) break;
		dir = QDir::cleanPath(dir + "/..");
	}
	for (QString const &gitignore : gitignores) {
		ui->comboBox_file->addItem(gitignore);
	}
}

void EditGitIgnoreDialog::updateRadioButtons()
{
	text_map_.clear();
	
	QRadioButton *buttons[] = {
		ui->radioButton_1,
		ui->radioButton_2,
		ui->radioButton_3,
		ui->radioButton_4,
	};
	size_t button_index = 0;
	
	{
		for (QRadioButton *button : buttons) {
			button->setText({});
			button->setVisible(false);
		}
	}
	ui->label_error->setText(validation_error_);
	ui->label_error->setVisible(!validation_error_.isEmpty());
	ui->pushButton_2->setEnabled(false);
	ui->pushButton_edit_file->setEnabled(false);
	if (!validation_error_.isEmpty() || gitignorePath().isEmpty() || files_.isEmpty()) return;
	
	auto SetText = [&](QString text) {
		if (!text.isEmpty() && button_index < std::size(buttons)) {
			QRadioButton *button = buttons[button_index++];
			text_map_[button] = text;
			button->setText(text);
			button->setVisible(true);
		}
	};

	auto SetPath = [&](QString path) {
		SetText(patternForPath(path));
	};
	
	QString file = files_.front();
	if (files_.size() == 1) {
		SetPath(file);
		
		QFileInfo info(file);
		if (!info.suffix().isEmpty()) {
			SetText("*." + escapeIgnorePattern(info.suffix()));
		}
		
		int i = file.indexOf('/');
		if (i > 0) {
			SetPath(file.mid(0, i + 1));
			int j = file.lastIndexOf('/');
			if (i < j) {
				SetPath(file.mid(0, j + 1));
			}
		}
	} else if (files_.size() > 1) {
		SetText(tr("%1 selected files").arg(files_.size()));
		ui->radioButton_1->setToolTip(this->files().join('\n'));
	}

	ui->radioButton_1->setFocus();
	ui->radioButton_1->click();
	bool valid = !this->files().isEmpty();
	ui->pushButton_2->setEnabled(valid);
	ui->pushButton_edit_file->setEnabled(valid);
}

QString EditGitIgnoreDialog::patternForPath(QString const &path) const
{
	if (gitignorePath().isEmpty()) return {};
	QDir destdir(QFileInfo(gitignorePath()).absolutePath());
	QString relative = destdir.relativeFilePath(QDir(working_dir_).absoluteFilePath(path));
	if (relative == "." || isOutsideDirectory(relative)) return {};
	// Anchor and escape literal paths for both single and multiple selections.
	return "/" + escapeIgnorePattern(relative) + (path.endsWith('/') ? "/" : "");
}

QString EditGitIgnoreDialog::gitignorePath() const
{
	return ui->comboBox_file->currentText();
}

QStringList EditGitIgnoreDialog::files() const
{
	if (!validation_error_.isEmpty() || gitignorePath().isEmpty()) return {};
	if (files_.size() == 1) {
		std::array<QRadioButton *, 4> arr = {ui->radioButton_1, ui->radioButton_2, ui->radioButton_3, ui->radioButton_4};
		for (auto button : arr) {
			if (button->isChecked()) {
				auto it = text_map_.find(button);
				if (it != text_map_.end()) {
					return {it->second};
				}
			}
		}
	} else if (files_.size() > 1) {
		QStringList ret;
		for (QString const &file : files_) {
			QString path = patternForPath(file);
			if (path.isEmpty()) return {};
			ret.push_back(path);
		}
		return ret;
	}
	return {};
}

void EditGitIgnoreDialog::accept()
{
	QStringList patterns = files();
	if (patterns.isEmpty()) return;
	QByteArray text;
	if (!TextEditDialog::readFile(this, gitignorePath(), &text)) return;
	if (!text.isEmpty() && !text.endsWith('\n')) text += '\n';
	text += patterns.join('\n').toUtf8() + '\n';
	if (TextEditDialog::writeFile(this, gitignorePath(), text)) {
		QDialog::accept();
	}
}

void EditGitIgnoreDialog::on_pushButton_edit_file_clicked()
{
	QStringList list = files();
	if (list.isEmpty()) return;
	if (TextEditDialog::editFile(this, gitignorePath(), ".gitignore", list.join('\n') + '\n')) {
		QDialog::accept();
	}
}

void EditGitIgnoreDialog::on_comboBox_file_currentTextChanged(const QString &)
{
	updateRadioButtons();
}

