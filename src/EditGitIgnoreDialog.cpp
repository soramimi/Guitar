#include "EditGitIgnoreDialog.h"
#include "MainWindow.h"
#include "TextEditDialog.h"
#include "ui_EditGitIgnoreDialog.h"
#include <QFileInfo>

EditGitIgnoreDialog::EditGitIgnoreDialog(MainWindow *parent, QString const &gitignore_path, QString const &file)
	: QDialog(parent)
	, ui(new Ui::EditGitIgnoreDialog)
	, gitignore_path(gitignore_path)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	QFileInfo info(file);
	
	ui->radioButton_1->setVisible(false);
	ui->radioButton_2->setVisible(false);
	ui->radioButton_3->setVisible(false);
	ui->radioButton_4->setVisible(false);
	
	auto SetText = [&](QRadioButton *button, QString const &text) {
		if (!text.isEmpty()) {
			text_map_[button] = text;
			button->setText(text);
			button->setVisible(true);
		}
	};
	
	SetText(ui->radioButton_1, file);
	SetText(ui->radioButton_2, "*." + info.suffix());

	int i = file.indexOf('/');
	if (i > 0) {
		SetText(ui->radioButton_3, file.mid(0, i + 1));
		int j = file.lastIndexOf('/');
		if (i < j) {
			SetText(ui->radioButton_4, file.mid(0, j + 1));
		}
	}

	ui->radioButton_1->click();
}

EditGitIgnoreDialog::~EditGitIgnoreDialog()
{
	delete ui;
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
	if (TextEditDialog::editFile(this, gitignore_path, ".gitignore", text() + '\n')) {
		mainwindow()->updateCurrentFilesList(mainwindow()->frame());
		done(QDialog::Rejected);
	}
}
