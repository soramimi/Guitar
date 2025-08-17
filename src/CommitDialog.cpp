#include "CommitDialog.h"
#include "ui_CommitDialog.h"
#include "ApplicationGlobal.h"
#include "ConfigSigningDialog.h"
#include "MainWindow.h"
#include "GenerateCommitMessageDialog.h"
#include <QDir>

CommitDialog::CommitDialog(MainWindow *parent, QString const &reponame, GitUser const &user, gpg::Data const &key, QString const &previousMessage)
	: QDialog(parent)
	, ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);
	
	connect(&generator_, &GenerateCommitMessageThread::ready, this, &CommitDialog::onReady);
	generator_.start();
	
	updateUI(true);

	key_ = key;
	previousMessage_ = previousMessage;

	ui->label_reponame->setText(reponame);
	ui->label_commit_author->setText(user.name);
	ui->label_commit_mail->setText(user.email);

	ui->checkbox_amend->setChecked(false);
	if (previousMessage_.isEmpty()) {
		ui->checkbox_amend->hide();
	}

	updateSigningInfo();

	ui->plainTextEdit->setFocus();
}

CommitDialog::~CommitDialog()
{
	generator_.stop();
	delete ui;
}

void CommitDialog::updateUI(bool enable)
{
	bool is_ai_enabled = enable && global->appsettings.generate_commit_message_by_ai;
	bool is_detailed_enabled = is_ai_enabled && !ui->plainTextEdit->toPlainText().isEmpty();
	
	ui->pushButton_generate_with_ai->setEnabled(is_ai_enabled);
}

MainWindow *CommitDialog::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

void CommitDialog::updateSigningInfo()
{
	GitRunner g = mainwindow()->git();

	GitSignPolicy pol = g.signPolicy(GitSource::Default);
	if (!key_.id.isEmpty()) {
		if (pol == GitSignPolicy::True) {
			ui->groupBox_gpg_sign->setCheckable(false);
		} else {
			ui->groupBox_gpg_sign->setCheckable(true);
			ui->groupBox_gpg_sign->setChecked(false);
		}
		ui->label_sign_id->setText(key_.id);
		ui->label_sign_name->setText(key_.name);
		ui->label_sign_mail->setText(key_.mail);
	} else {
		ui->groupBox_gpg_sign->setChecked(false);
		ui->groupBox_gpg_sign->setEnabled(false);
	}
}

bool CommitDialog::isSigningEnabled() const
{
	return ui->groupBox_gpg_sign->isChecked();
}

bool CommitDialog::isAmend() const
{
	return ui->checkbox_amend->isChecked();
}

void CommitDialog::setText(QString const &text)
{
	ui->plainTextEdit->setPlainText(text);
	QTextCursor cur = ui->plainTextEdit->textCursor();
	cur.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
	ui->plainTextEdit->setTextCursor(cur);
}

QString CommitDialog::text() const
{
	return ui->plainTextEdit->toPlainText();
}

void CommitDialog::onReady(GeneratedCommitMessage const &msg)
{
	GlobalRestoreOverrideCursor();
	updateUI(true);
	
	if (msg && !msg.messages().empty()) {
		std::vector<std::string> lines;
		lines.push_back(commit_message_);
		lines.push_back({});
		for (std::string const &line : msg.messages()) {
			lines.push_back(line);
		}
		auto Join = [](std::vector<std::string> lines, char c){
			std::string ret;
			for (auto const &line : lines) {
				if (!ret.empty()) {
					ret.push_back(c);
				}
				ret.append(line);
			}
			return ret;
		};
		std::string text = Join(lines, '\n');
		setText(QString::fromStdString(text));
	}
}

void CommitDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
			event->accept();
			accept();
			return;
		}
	}
	QDialog::keyPressEvent(event);
}

void CommitDialog::on_pushButton_config_signing_clicked()
{
	ConfigSigningDialog dlg(this, mainwindow(), true);
	if (dlg.exec() == QDialog::Accepted) {
		updateSigningInfo();
	}
}

void CommitDialog::on_checkbox_amend_stateChanged(int state)
{
	setText(state == Qt::Checked ? previousMessage_ : QString(""));
}

void CommitDialog::on_pushButton_generate_with_ai_clicked()
{
	diff_ = CommitMessageGenerator::diff_head(global->mainwindow->git());
	
	GenerateCommitMessageDialog dlg(this, global->appsettings.ai_model.model_name());
	dlg.show();
	dlg.generate(diff_);
	if (dlg.exec() == QDialog::Accepted) {
		QStringList list = dlg.message();
		if (!list.isEmpty()) {
			QString text;
			int n = list.size();
			for (int i = 0; i < n; i++) {
				if (i > 0) {
					text.append('\n');
				}
				QString line = list.at(i);
				if (n > 1 && !line.endsWith('.')) {
					line += '.';
				}
				text.append(line);
			}
			setText(text);
		}
	}
	ui->plainTextEdit->setFocus();
}

void CommitDialog::on_plainTextEdit_textChanged()
{
	updateUI(true);
}

