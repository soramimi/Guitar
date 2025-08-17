#include "RepositoryPropertyDialog.h"
#include "ui_RepositoryPropertyDialog.h"
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "common/misc.h"
#include <QClipboard>
#include <QItemDelegate>
#include <QMenu>
#include <QMessageBox>

struct RepositoryPropertyDialog::Private {
	GitRunner git;
	std::vector<GitRemote> remotes;
	RepositoryInfo repository;
	bool remote_changed = false;
	bool name_changed = false;
};

RepositoryPropertyDialog::RepositoryPropertyDialog(MainWindow *parent, GitRunner g, RepositoryInfo const &item, bool open_repository_menu)
	: QDialog(parent)
	, ui(new Ui::RepositoryPropertyDialog)
	, m(new Private)
{
	ui->setupUi(this);
	
	m->git = g;
	
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->repository = item;

	ui->label_editable_name->setText(m->repository.name);
	ui->label_editable_name->setVisible(true);
	ui->lineEdit_name->setText(m->repository.name);
	ui->lineEdit_name->setVisible(false);
	ui->lineEdit_local_dir->setText(misc::normalizePathSeparator(m->repository.local_dir));
	
	connect(ui->tableWidget->itemDelegate(), &QItemDelegate::closeEditor, this, [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint){
		reflectRemotesTable();
	});
	
	ui->pushButton_close->setFocus();
	
	updateRemotesTable();
	
}

RepositoryPropertyDialog::~RepositoryPropertyDialog()
{
	delete ui;
	delete m;
}

MainWindow *RepositoryPropertyDialog::mainwindow()
{
	return global->mainwindow;
}

GitRunner RepositoryPropertyDialog::git()
{
	return m->git;
}

void RepositoryPropertyDialog::getRemotes_()
{
	GitRunner g = git();
	if (g.isValidWorkingCopy()) {
		g.remote_v(&m->remotes);
	}
}

void RepositoryPropertyDialog::updateRemotesTable()
{
	ui->tableWidget->clear();
	m->remotes.clear();
	getRemotes_();
	QString url;
	QString alturl;
	int rows = m->remotes.size();
	ui->tableWidget->setColumnCount(2);
	ui->tableWidget->setRowCount(rows);
	auto newQTableWidgetItem = [](QString const &text){
		auto *item = new QTableWidgetItem;
		item->setSizeHint(QSize(20, 20));
		item->setText(text);
		return item;
	};
	auto SetHeaderItem = [&](int col, QString const &text){
		ui->tableWidget->setHorizontalHeaderItem(col, newQTableWidgetItem(text));
	};
	SetHeaderItem(0, tr("Name"));
	SetHeaderItem(1, tr("URL"));
	for (int row = 0; row < rows; row++) {
		GitRemote const &r = m->remotes[row];
		url = r.url_fetch;
		auto SetItem = [&](int col, QString const &text, bool editable){
			auto item = newQTableWidgetItem(text);
			auto flags = item->flags();
			flags &= ~Qt::ItemIsEditable;
			item->setFlags(flags);
			ui->tableWidget->setItem(row, col, item);
		};
		SetItem(0, r.name, false);
		SetItem(1, r.url(), true);
	}
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

void RepositoryPropertyDialog::reflectRemotesTable()
{
	int rows = m->remotes.size();
	for (int row = 0; row < rows; row++) {
		GitRemote *r = &m->remotes[row];
		QString url = ui->tableWidget->item(row, 1)->text();
		if (r->url() != url) {
			r->set_url(url);
			git().setRemoteURL(*r);
		}
	}
}

const std::vector<GitRemote> *RepositoryPropertyDialog::remotes() const
{
	return &m->remotes;
}

void RepositoryPropertyDialog::setSshKey_(const QString &sshkey)
{
	git().setSshKey(sshkey);
}

/**
 * @brief リポジトリプロパティダイアログ
 * @param remote
 * @param op
 * @return
 */
bool RepositoryPropertyDialog::execEditRemoteDialog(GitRemote *remote, EditRemoteDialog::Operation op)
{
	auto const *list = remotes();
	if (op == EditRemoteDialog::RemoteSet) {
		int row = ui->tableWidget->currentRow();
		if (row >= 0 && row < list->size()) {
			*remote = list->at(row);
		}
	} else {
		*remote = GitRemote();
	}

	if (remote->name.isEmpty() && list->empty()) {
		op = EditRemoteDialog::RemoteAdd;
		remote->name = "origin";
	}

	EditRemoteDialog dlg(mainwindow(), op);
	dlg.setName(remote->name);
	dlg.setUrl(remote->url_fetch);
	dlg.setSshKey(remote->ssh_key);
	if (dlg.exec() == QDialog::Accepted) {
		remote->name = dlg.name();
		remote->set_url(dlg.url());
		remote->ssh_key = dlg.sshKey();
		GitRunner g = git();
		if (op == EditRemoteDialog::RemoteAdd) {
			bool ok = true;
			for (GitRemote const &r : *list) {
				if (r.name == remote->name) {
					qDebug() << "remote add: Already exists" << remote->name;
					ok = false;
					break;
				}
			}
			if (ok) {
				g.addRemoteURL(*remote);
			}
		} else if (op == EditRemoteDialog::RemoteSet) {
			g.setRemoteURL(*remote);
		}

		QString localdir = ui->lineEdit_local_dir->text();
		mainwindow()->changeSshKey(localdir, remote->ssh_key, true);
		setSshKey_(remote->ssh_key);
		getRemotes_();

		updateRemotesTable();
		return true;
	}

	return false;
}

GitRemote RepositoryPropertyDialog::selectedRemote() const
{
	GitRemote remote;
	auto const *list = remotes();
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < list->size()) {
		remote = list->at(row);
	}
	return remote;
}

bool RepositoryPropertyDialog::isRemoteChanged() const
{
	return m->remote_changed;
}

bool RepositoryPropertyDialog::isNameChanged() const
{
    return m->name_changed;
}

QString RepositoryPropertyDialog::getName()
{
    return m->repository.name;
}

void RepositoryPropertyDialog::on_pushButton_remote_add_clicked()
{
	GitRemote r;
	if (execEditRemoteDialog(&r, EditRemoteDialog::RemoteAdd)) {
		m->remote_changed = true;
	}
}

void RepositoryPropertyDialog::on_pushButton_remote_edit_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) {
		ui->tableWidget->setCurrentCell(0, 0);
	}
	GitRemote remote = selectedRemote();
	if (execEditRemoteDialog(&remote, EditRemoteDialog::RemoteSet)) {
		m->remote_changed = true;
	}
}

void RepositoryPropertyDialog::on_pushButton_remote_remove_clicked()
{
	GitRemote remote = selectedRemote();
	if (!remote.name.isEmpty()) {
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the remote '%1' from the repository '%2'?").arg(remote.name).arg(m->repository.name), QMessageBox::Ok, QMessageBox::Cancel);
		if (r == QMessageBox::Ok) {
			GitRunner g = git();
			g.removeRemote(remote.name);
			updateRemotesTable();
			m->remote_changed = true;
		}
	}
}

void RepositoryPropertyDialog::setNameEditMode(bool f)
{
	ui->lineEdit_name->setVisible(f);
	ui->label_editable_name->setVisible(!f);
	ui->pushButton_edit_name->setText(f ? tr("Save") : tr("Edit Name"));
	if (f) {
		ui->lineEdit_name->setText(m->repository.name);
		ui->lineEdit_name->setFocus();
	} else {
		if (!ui->lineEdit_name->text().isEmpty()) {
			m->repository.name = ui->lineEdit_name->text();
			ui->label_editable_name->setText(m->repository.name);
			m->name_changed = true;
		}
		ui->pushButton_close->setFocus();
	}
}

bool RepositoryPropertyDialog::isNameEditMode() const
{
	return ui->lineEdit_name->isVisible();
}

void RepositoryPropertyDialog::on_pushButton_edit_name_clicked()
{
	setNameEditMode(!isNameEditMode());
}

void RepositoryPropertyDialog::reject()
{
	if (isNameEditMode()) {
		setNameEditMode(false);
	} else {
		QDialog::reject();
	}
}

void RepositoryPropertyDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	on_pushButton_remote_edit_clicked();
}

