#include "GenerateCommitMessageDialog.h"
#include "ui_GenerateCommitMessageDialog.h"
#include "GenerateCommitMessageThread.h"
#include "MainWindow.h"
#include "MySettings.h"
#include <QMessageBox>
#include <ai/CommitMessageGenerator.h>
#include <ai/GenerativeAI.h>
#include <common/joinpath.h>
#include <common/q/helper.h>

namespace {
QListWidgetItem *new_QListWidgetItem(QString const &text)
{
	auto *p = new QListWidgetItem(text);
	p->setSizeHint({20, 20});
	return p;
}
}

struct GenerateCommitMessageDialog::Private {
	std::vector<GenerativeAI::Model> ai_models;
	CommitMessageGenerator::CommitPair commits;
	GenerateCommitMessageThread generator;
	QStringList checked_items;
	std::string diff;
	std::string status_s_u;
};

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageDialog)
	, m(new Private)
{
	ui->setupUi(this);

	int default_index;
	std::vector<GenerativeAI::ModelConf> models;
	QString path = global->aimodels_json_path();
	auto opt = GenerativeAI::ModelConf::load(path.toStdString().c_str());
	if (opt) {
		std::string guid;
		{
			MySettings s;
			s.beginGroup("AI");
			guid = s.value("FirstChoiceGUID").toString().toStdString();
			s.endGroup();
		}
		models = std::move(*opt);
		for (size_t i = 0; i < models.size(); i++) {
			GenerativeAI::ModelConf const &conf = models[i];
			if (conf.guid == guid) {
				default_index = (int)i;
			}
		}
	}
	
	updateModels(models, default_index);

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

void GenerateCommitMessageDialog::updateModels(std::vector<GenerativeAI::ModelConf> const &models, int default_index)
{
	m->ai_models.clear();
	m->ai_models.reserve(models.size());
	ui->comboBox_ai_models->clear();
	for (size_t i = 0; i < models.size(); i++) {
		GenerativeAI::ModelConf const &conf = models[i];
		m->ai_models.push_back(conf.model);
		ui->comboBox_ai_models->addItem((QS)conf.name);
	}
	ui->comboBox_ai_models->setCurrentIndex(default_index);
}

std::tuple<GenerativeAI::Model, GenerativeAI::Credential> GenerateCommitMessageDialog::ai_model() const
{
	int index = ui->comboBox_ai_models->currentIndex();

	auto QueryApiKey = [](std::string const &symbol, bool env)-> std::string {
		if (env) {
			char const *e = std::getenv(symbol.c_str());
			if (e) return e;
		} else {
			AiApiKeys ai_api_keys;
			ai_api_keys.load((std::string)api_key_obfuscation_key, nullptr);
			auto opt = ai_api_keys.get_api_key(symbol);
			if (opt) return opt->api_key;
		}
		return {};
	};
	
	{
		QString guid;
		{
			MySettings s;
			s.beginGroup("AI");
			guid = s.value("FirstChoiceGUID").toString();
			s.endGroup();
		}
		
		QString path = global->aimodels_json_path();
		std::optional<std::vector<GenerativeAI::ModelConf>> opt = GenerativeAI::ModelConf::load(path.toStdString().c_str());
		if (opt) {
			if (index >= 0 && (size_t)index < opt->size()) {
				GenerativeAI::ModelConf const &conf = (*opt)[index];
				GenerativeAI::Credential cred;
				cred.api_key = QueryApiKey(conf.api_key_symbol, conf.api_key_method == GenerativeAI::key_store_environment);
				return {conf.model, cred};
			}
		}
	}

	return {*global->appsettings.ai_model, {}};
}

void GenerateCommitMessageDialog::_generate(std::string const &diff, std::string const &status_s_u)
{
	m->diff = diff;
	m->status_s_u = status_s_u;

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
	
	auto [model, cred] = ai_model();
	m->generator.request(model, cred, diff, status_s_u, hint);
}

void GenerateCommitMessageDialog::generate()
{
	GitRunner g = global->mainwindow->git();
	std::string diff = CommitMessageGenerator::make_diff(g, m->commits);
	std::string status_s_u;
	g.status_s_u(&status_s_u);
	_generate(diff, status_s_u);
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

