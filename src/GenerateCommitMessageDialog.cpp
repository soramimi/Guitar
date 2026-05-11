#include "GenerateCommitMessageDialog.h"
#include "ui_GenerateCommitMessageDialog.h"
#include "ApplicationGlobal.h"
#include "CommitMessageGenerator.h"
#include "GenerateCommitMessageThread.h"
#include "MainWindow.h"
#include "common/q/helper.h"
#include <QMessageBox>

struct GenerateCommitMessageDialog::Private {
	std::vector<GenerativeAI::Model> ai_models;
	CommitMessageGenerator::CommitPair commits;
	GenerateCommitMessageThread generator;
	QStringList checked_items;
	std::string diff;
};

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent, std::vector<GenerativeAI::Model> const &models, int default_index)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageDialog)
	, m(new Private)
{
	ui->setupUi(this);

	init_ai_models(models, default_index);

	m->generator.start();
	
	connect(&m->generator, &GenerateCommitMessageThread::ready, this, &GenerateCommitMessageDialog::onReady);

	ui->checkBox_hint->setCheckState(Qt::Unchecked);
	ui->lineEdit_hint->setEnabled(false);

	// 実験的機能: 追加のヒント入力欄はとりあえず隠しておく
	ui->frame_additional_hint->setVisible(false);
}

GenerateCommitMessageDialog::~GenerateCommitMessageDialog()
{
	m->generator.stop();
	delete ui;
	delete m;
}

void GenerateCommitMessageDialog::setCommitIDs(CommitMessageGenerator::CommitPair const &commits)
{
	m->commits = commits;
}

void GenerateCommitMessageDialog::init_ai_models(std::vector<GenerativeAI::Model> const &models, int default_index)
{
	m->ai_models = models;
	ui->comboBox_ai_models->clear();
	for (int i = 0; i < m->ai_models.size(); i++) {
		GenerativeAI::Model const &model = m->ai_models[i];
		ui->comboBox_ai_models->addItem((QS)model.model_uri().string);
	}
	ui->comboBox_ai_models->setCurrentIndex(default_index);
}

GenerativeAI::Model const &GenerateCommitMessageDialog::ai_model() const
{
	int index = ui->comboBox_ai_models->currentIndex();
	if (index >= 0 && index < m->ai_models.size()) {
		return m->ai_models[index];
	}
	return global->appsettings.ai_model;
}

void GenerateCommitMessageDialog::generate(std::string const &diff)
{
	m->diff = diff;

	// experimental: additional hint for AI generation
	std::string hint;
	if (ui->checkBox_hint->isChecked()) {
		hint = ui->lineEdit_hint->text().toStdString();
	}
	
	GlobalSetOverrideWaitCursor();

	m->checked_items = message();

	ui->listWidget->clear();

	for (QString const &s : m->checked_items) {
		auto *item = new_QListWidgetItem(s);
		item->setCheckState(Qt::Checked);
		ui->listWidget->addItem(item);
	}

	ui->pushButton_regenerate->setEnabled(false);
	
	m->generator.request(diff, ai_model(), hint);
}

void GenerateCommitMessageDialog::generate()
{
	std::string diff = CommitMessageGenerator::make_diff(global->mainwindow->git(), m->commits);
	generate(diff);
}

std::string GenerateCommitMessageDialog::diffText() const
{
	return m->diff;
}

QStringList GenerateCommitMessageDialog::message() const
{
	QStringList list;
	int n = ui->listWidget->count();
	for (int i = 0; i < n; i++) {
		auto *item = ui->listWidget->item(i);
		if (item->checkState() == Qt::Checked) {
			list.append(item->text());
		}
	}
	return list;
}

void GenerateCommitMessageDialog::on_pushButton_regenerate_clicked()
{
	generate();
}

void GenerateCommitMessageDialog::onReady(const GeneratedCommitMessage &result)
{
	GlobalRestoreOverrideCursor();

	if (result) {
		for (std::string const &s : result.messages()) {
			QListWidgetItem *item = new_QListWidgetItem((QS)s);
			item->setCheckState(Qt::Unchecked);
			ui->listWidget->addItem(item);
		}
		ui->listWidget->setCurrentRow(0);
	} else {
		std::string text = result.error_status() + "\n\n" + result.error_message();
		QMessageBox::warning(this, tr("Failed to generate commit message."), QString::fromStdString(text));
	}
	
	ui->pushButton_regenerate->setEnabled(true);	
}

void GenerateCommitMessageDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	item->setCheckState(Qt::Checked);
	done(QDialog::Accepted);
}

void GenerateCommitMessageDialog::done(int stat)
{
	if (stat == QDialog::Accepted) {
		QStringList list = message();
		if (list.empty()) {
			auto *item = ui->listWidget->currentItem();
			if (item) {
				item->setCheckState(Qt::Checked);
			}
		}
	}
	QDialog::done(stat);
}


void GenerateCommitMessageDialog::on_checkBox_hint_checkStateChanged(const Qt::CheckState &arg1)
{
	ui->lineEdit_hint->setEnabled(arg1 == Qt::Checked);
}

