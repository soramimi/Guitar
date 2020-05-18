#include "RepositoryPropertyDialog.h"
#include "ui_RepositoryPropertyDialog.h"
#include "MainWindow.h"
#include "common/misc.h"
#include <QClipboard>
#include <QMenu>
#include <QMessageBox>

RepositoryPropertyDialog::RepositoryPropertyDialog(BasicMainWindow *parent, Git::Context const *gcx, GitPtr const &g, RepositoryItem const &item, bool open_repository_menu)
	: BasicRepositoryDialog(parent, g)
	, ui(new Ui::RepositoryPropertyDialog)
	, gcx(gcx)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	repository = item;

	ui->groupBox_remote->setVisible(open_repository_menu);

	ui->label_name->setText(repository.name);
	ui->lineEdit_local_dir->setText(misc::normalizePathSeparator(repository.local_dir));

	updateRemotesTable();
}

RepositoryPropertyDialog::~RepositoryPropertyDialog()
{
	delete ui;
}

void RepositoryPropertyDialog::updateRemotesTable()
{
	BasicRepositoryDialog::updateRemotesTable(ui->tableWidget);
}

void RepositoryPropertyDialog::toggleRemoteMenuActivity()
{
	ui->groupBox_remote->setVisible(!ui->groupBox_remote->isVisible());
}

void BasicMainWindow::changeSshKey(QString const &localdir, QString const &sshkey)
{
	bool changed = false;
	QList<RepositoryItem> *repos = getReposPtr();
	for (int i = 0; i < repos->size(); i++) {
		RepositoryItem *item = &(*repos)[i];
		if (item->local_dir == localdir) {
			item->ssh_key = sshkey;
			changed = true;
		}
	}
	if (changed) {
		saveRepositoryBookmarks();
	}
}

bool RepositoryPropertyDialog::execEditRemoteDialog(Git::Remote *remote, EditRemoteDialog::Operation op)
{
	auto const *list = remotes();
	if (op == EditRemoteDialog::RemoteSet) {
		int row = ui->tableWidget->currentRow();
		if (row >= 0 && row < list->size()) {
			*remote = list->at(row);
		}
	} else {
		*remote = Git::Remote();
	}

	if (remote->name.isEmpty() && list->isEmpty()) {
		op = EditRemoteDialog::RemoteAdd;
		remote->name = "origin";
	}

	EditRemoteDialog dlg(mainwindow(), op, gcx);
	dlg.setName(remote->name);
	dlg.setUrl(remote->url);
	dlg.setSshKey(remote->ssh_key);
	if (dlg.exec() == QDialog::Accepted) {
		remote->name = dlg.name();
		remote->url = dlg.url();
		remote->ssh_key = dlg.sshKey();
		GitPtr g = git();
		if (op == EditRemoteDialog::RemoteAdd) {
			bool ok = true;
			for (Git::Remote const &r : *list) {
				if (r.name == remote->name) {
					qDebug() << "remote add: Already exists" << remote->name;
					ok = false;
					break;
				}
			}
			if (ok) {
				g->addRemoteURL(*remote);
			}
		} else if (op == EditRemoteDialog::RemoteSet) {
			g->setRemoteURL(*remote);
		}

		// wip
		QString localdir = ui->lineEdit_local_dir->text();
		mainwindow()->changeSshKey(localdir, remote->ssh_key);
		setSshKey_(remote->ssh_key);
		getRemotes_();

		updateRemotesTable();
		return true;
	}

	return false;
}

Git::Remote RepositoryPropertyDialog::selectedRemote() const
{
	Git::Remote remote;
	auto const *list = remotes();
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < list->size()) {
		remote = list->at(row);
	}
	return remote;
}

bool RepositoryPropertyDialog::isRemoteChanged() const
{
	return remote_changed;
}

void RepositoryPropertyDialog::on_pushButton_remote_add_clicked()
{
	Git::Remote r;
	if (execEditRemoteDialog(&r, EditRemoteDialog::RemoteAdd)) {
		remote_changed = true;
	}
}

void RepositoryPropertyDialog::on_pushButton_remote_edit_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) {
		ui->tableWidget->setCurrentCell(0, 0);
	}
	Git::Remote remote = selectedRemote();
	if (execEditRemoteDialog(&remote, EditRemoteDialog::RemoteSet)) {
		remote_changed = true;
	}
}

void RepositoryPropertyDialog::on_pushButton_remote_remove_clicked()
{
	Git::Remote remote = selectedRemote();
	if (!remote.name.isEmpty()) {
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the remote '%1' from the repository '%2' ?").arg(remote.name).arg(repository.name), QMessageBox::Ok, QMessageBox::Cancel);
		if (r == QMessageBox::Ok) {
			GitPtr g = git();
			g->removeRemote(remote.name);
			updateRemotesTable();
			remote_changed = true;
		}
	}
}

void RepositoryPropertyDialog::on_pushButton_remote_menu_clicked()
{
	toggleRemoteMenuActivity();
}
