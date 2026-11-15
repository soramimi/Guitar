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
		files_.push_back(f);
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
	if (files_.empty()) return;
	QString file = files_.front();
	
	QDir workdir(working_dir_);
	// Keep repository paths lexical: resolving symlinks can leave the worktree,
	// and the selected file may already have been deleted.
	QString dir = QFileInfo(workdir.absoluteFilePath(file)).absolutePath();
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
	
	QRadioButton *buttons[] = {
		ui->radioButton_1,
		ui->radioButton_2,
		ui->radioButton_3,
		ui->radioButton_4,
	};
	int button_index = 0;
	
	{
		for (QRadioButton *button : buttons) {
			button->setText({});
			button->setVisible(false);
		}
	}
	
	auto SetText = [&](QString text) {
		if (!text.isEmpty() && button_index < std::size(buttons)) {
			QRadioButton *button = buttons[button_index++];
			text_map_[button] = text;
			button->setText(text);
			button->setVisible(true);
		}
	};

	auto SetPath = [&](QString path) {
		bool directory = path.endsWith('/');
		path = destdir.relativeFilePath(QDir(working_dir_).absoluteFilePath(path));
		if (path != "." && !isOutsideDirectory(path)) {
			// Anchor individual paths; a bare basename also matches descendants.
			SetText("/" + escapeIgnorePattern(path) + (directory ? "/" : ""));
		}
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
		QFileInfo info(file);
		QString path = info.dir().filePath({});
		path = destdir.relativeFilePath(QDir(working_dir_).absoluteFilePath(path));
		QString suffix = QString(" <%1 files>").arg(files_.size());
		if (path == ".") {
			path = "/";
		} else {
			path = "/" + path;
			suffix = "/" + suffix;
		}
		SetText(escapeIgnorePattern(path) + suffix);
	} else {
		Q_ASSERT(0);
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

QStringList EditGitIgnoreDialog::files() const
{
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
		QDir destdir(QFileInfo(gitignorePath()).absolutePath());
		for (QString const &file : files_) {
			QString path = destdir.relativeFilePath(QDir(working_dir_).absoluteFilePath(file));
			if (path == ".") {
				path = "/";
			} else {
				path = "/" + path;
			}
			ret.push_back(path);
		}
		return ret;
	}
	return {};
}

void EditGitIgnoreDialog::on_pushButton_edit_file_clicked()
{
	QStringList list = files();
	if (list.size() == 1) {
		QString file = list.front();
		if (TextEditDialog::editFile(this, gitignorePath(), ".gitignore", file + '\n')) {
			mainwindow()->updateCurrentFileList();
			done(QDialog::Rejected);
		}
	}
}

void EditGitIgnoreDialog::on_comboBox_file_currentTextChanged(const QString &)
{
	updateRadioButtons();
}

