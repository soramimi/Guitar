#include "GenerateCommitMessageDialog.h"
#include "ui_GenerateCommitMessageDialog.h"
#include "ApplicationGlobal.h"
#include "CommitMessageGenerator.h"
#include "MainWindow.h"
#include "OverrideWaitCursor.h"
#include <condition_variable>
#include <thread>
#include <mutex>

struct GenerateCommitMessageDialog::Private {
	std::mutex mutex;
	std::thread thread;
	std::condition_variable cv;
	bool requested = false;
	bool interrupted = false;
	CommitMessageGenerator gen;
};

GenerateCommitMessageDialog::GenerateCommitMessageDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GenerateCommitMessageDialog)
	, m(new Private)
{
	ui->setupUi(this);
	
	connect(this, &GenerateCommitMessageDialog::ready, this, &GenerateCommitMessageDialog::onReady);	
	
	m->thread = std::thread([this] {
		while (1) {
			bool requested = false;
			{
				std::unique_lock lock(m->mutex);
				if (m->interrupted)	break;
				m->cv.wait(lock);
				std::swap(requested, m->requested);
			}
			if (requested) {
				OverrideWaitCursor;
				QStringList list = m->gen.generate(global->mainwindow->git());
				emit ready(list);
			}
		}
		generate();
	});	
}

GenerateCommitMessageDialog::~GenerateCommitMessageDialog()
{
	m->interrupted = true;
	m->cv.notify_all();
	if (m->thread.joinable()) {
		m->thread.join();
	}
	delete ui;
	delete m;
}

QString GenerateCommitMessageDialog::text() const
{
	return ui->listWidget->currentItem()->text();
}

void GenerateCommitMessageDialog::generate()
{
	ui->listWidget->clear();
	ui->pushButton_regenerate->setEnabled(false);
	
	m->interrupted = false;
	m->requested = true;
	m->cv.notify_all();
}

void GenerateCommitMessageDialog::on_pushButton_regenerate_clicked()
{
	generate();
}

void GenerateCommitMessageDialog::onReady(const QStringList &list)
{
	if (!list.isEmpty()) {
		ui->listWidget->addItems(list);
		ui->listWidget->setCurrentRow(0);
	}
	
	ui->pushButton_regenerate->setEnabled(true);	
}
