#include "SshDialog.h"
#include "ui_SshDialog.h"
#include "../common/joinpath.h"
#include "Quissh.h"
#include <QFileIconProvider>
#include <QDebug>
#include <QKeyEvent>
#include <QDir>

static inline int x_stricmp(char const *s1, char const *s2)
{
#ifdef _WIN32
	return ::stricmp(s1, s2);
#else
	return ::strcasecmp(s1, s2);
#endif
}

static inline int x_strnicmp(char const *s1, char const *s2, size_t n)
{
#ifdef _WIN32
	return ::strnicmp(s1, s2, n);
#else
	return ::strncasecmp(s1, s2, n);
#endif
}

struct SshDialog::Private {
	Quissh::AuthVar auth;
	SshConnection *ssh = nullptr;
	std::string dir;
	std::vector<SshConnection::FileItem> files;
	QIcon icon_file;
	QIcon icon_folder;
};

SshDialog::SshDialog(QWidget *parent, SshConnection *ssh)
	: QDialog(parent)
	, ui(new Ui::Dialog)
	, m(new Private)
{
	ui->setupUi(this);

	m->ssh = ssh;

	setAuthMethod(PublicKey);

	QFileIconProvider iconprov;
	m->icon_file = iconprov.icon(QFileIconProvider::File);
	m->icon_folder = iconprov.icon(QFileIconProvider::Folder);

	m->dir = ".";

	m->auth = Quissh::PubkeyAuth(); // Default to public key authentication.

	updateFiles();
}

SshDialog::~SshDialog()
{
	doDisconnect();
	delete ui;
	delete m;
}

void SshDialog::setHostName(const QString &host)
{
	ui->lineEdit_host->setText(host);
}

void SshDialog::setPort(int port)
{
	ui->spinBox_port->setValue(port);
}

void SshDialog::setAuthMethod(AuthMethod method)
{
	bool f1 = ui->radioButton_key->blockSignals(true);
	bool f2 = ui->radioButton_pwd->blockSignals(true);
	ui->radioButton_key->setChecked(method == PublicKey);
	ui->radioButton_pwd->setChecked(method == Password);
	ui->frame_password_auth->setEnabled(method == Password);
	ui->radioButton_key->blockSignals(f1);
	ui->radioButton_pwd->blockSignals(f2);
}

void SshDialog::setUserName(const QString &uid)
{
	ui->lineEdit_uid->setText(uid);
}

void SshDialog::setPassword(const QString &pwd)
{
	ui->lineEdit_pwd->setText(pwd);
}

QString SshDialog::hostName() const
{
	return ui->lineEdit_host->text();
}

int SshDialog::port() const
{
	return ui->spinBox_port->value();
}

QString SshDialog::userName() const
{
	return ui->lineEdit_uid->text();
}

QString SshDialog::password() const
{
	return ui->lineEdit_pwd->text();
}

SshDialog::AuthMethod SshDialog::authMethod() const
{
	if (ui->radioButton_key->isChecked()) {
		return PublicKey;
	} else if (ui->radioButton_pwd->isChecked()) {
		return Password;
	}
	return PublicKey; // default
}

void SshDialog::updateFileList()
{
	ui->listWidget->clear();
	if (!isSftpConnected()) return;

	for (auto const &a : m->files) {
		QListWidgetItem *item = new QListWidgetItem();
		QString text = QString::fromStdString(a.name);
		if (a.isdir) {
			text += '/';
			item->setIcon(m->icon_folder);
		} else {
			item->setIcon(m->icon_file);
		}
		item->setText(text);
		ui->listWidget->addItem(item);
	}
}


void SshDialog::updateFiles()
{
	if (!isSftpConnected()) {
		updateFileList();
		return;
	}

	std::vector<std::string> parts;
	char const *begin = m->dir.c_str();
	char const *end = begin + m->dir.size();
	char const *left = begin;
	char const *right = begin;
	while (1) {
		int c = 0;
		if (right < end) {
			c = (unsigned char)*right;
		}
		if (c == '/' || c == '\\' || c == 0) {
			std::string_view v(left, right - left);
			if (v == "..") {
				if (!parts.empty()) {
					parts.pop_back();
				}
			} else if (!v.empty()) {
				parts.push_back(std::string(v));
			}
			if (c == 0) break;
			right++;
			left = right;
		} else {
			right++;
		}
	}
	std::string newdir;
	for (auto const &v : parts) {
		if (!newdir.empty()) {
			newdir += '/';
		}
		newdir.append(v);
	}
	if (newdir.empty()) {
		newdir = ".";
	}

	auto list = m->ssh->list(newdir.c_str());
	if (list) {
		m->files.clear();
		for (SshConnection::FileItem const &a : *list) {
			if (a.name == ".") continue;
			if (a.name == ".." && newdir == ".") continue;
			m->files.push_back(a);
		}
		SshConnection::sort(&m->files);
		m->dir = newdir;
		updateFileList();
	}

}

void SshDialog::onSelect()
{
	int row = ui->listWidget->currentRow();
	if (row >= 0 && row < m->files.size()) {
		auto const &a = m->files[row];
		if (a.isdir) {
			m->dir = m->dir / a.name;
			updateFiles();
		}
	}
}

QString SshDialog::selectedPath() const
{
	if (m->dir == ".") {
		return QString();
	}
	QString path = QString::fromStdString(m->dir);
	if (!path.endsWith('/')) {
		path += '/';
	}
	int row = ui->listWidget->currentRow();
	if (row >= 0 && row < m->files.size()) {
		path += QString::fromStdString(m->files[row].name);
	}
	return path;
}

void SshDialog::on_pushButton_ok_clicked()
{
	done(QDialog::Accepted);
}

void SshDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
		onSelect();
		return;
	}
	QDialog::keyPressEvent(event);
}

void SshDialog::updateUI()
{
	m->auth = Quissh::PubkeyAuth(); // Default to public key authentication.
	if (authMethod() == Password) {
		Quissh::PasswdAuth a;
		a.uid = userName().toStdString();
		a.pwd = password().toStdString();
		m->auth = Quissh::PasswdAuth(a); // Use password authentication if selected.
	}
}

void SshDialog::doConnect()
{
	std::string uid;
	std::string pwd;
	bool passwd = std::holds_alternative<Quissh::PasswdAuth>(m->auth);
	if (passwd) {
		uid = std::get<Quissh::PasswdAuth>(m->auth).uid;
		pwd = std::get<Quissh::PasswdAuth>(m->auth).pwd;
	}
	if (m->ssh->connect(hostName().toStdString().c_str(), port(), passwd, uid, pwd)) {
		if (m->ssh->open_sftp()) {
			updateFiles();
		}
	}
}

void SshDialog::doDisconnect()
{
	m->ssh->disconnect();

	updateFiles();

}

bool SshDialog::isSftpConnected() const
{
	return m->ssh && m->ssh->is_sftp_connected();
}

int SshDialog::exec()
{
	updateUI();
	return QDialog::exec();
}


void SshDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	onSelect();
}


void SshDialog::on_radioButton_key_clicked()
{
	ui->frame_password_auth->setEnabled(false);
}


void SshDialog::on_radioButton_pwd_clicked()
{
	ui->frame_password_auth->setEnabled(true);
}


void SshDialog::on_pushButton_connect_clicked()
{
	doConnect();
}


void SshDialog::on_pushButton_disconnect_clicked()
{
	doDisconnect();

}

//

struct SshConnection::Private {
	Quissh quissh;
	std::shared_ptr<Quissh::SFTP> sftp;
	std::shared_ptr<Quissh::CHANNEL> channel;
};

SshConnection::SshConnection()
	: m(new Private)
{
}

SshConnection::~SshConnection()
{
	disconnect();
	delete m;
}

bool SshConnection::open_channel()
{
	if (!m->channel) {
		m->channel = std::make_shared<Quissh::CHANNEL>(m->quissh);
		if (m->channel->open()) {
			return true;
		} else {
			qWarning() << "Failed to open SSH channel.";
			m->channel.reset();
			return false;
		}
	}
	return false;
}

void SshConnection::close_channel()
{
	if (m->channel) {
		m->channel->close();
	}
	m->channel.reset();
}

std::optional<std::string> SshConnection::exec(const char *command)
{
	std::vector<char> vec;
	auto writer = [&](char const *ptr, int len){
		vec.insert(vec.end(), ptr, ptr + len);
		return true;
	};
	if (m->quissh.exec(command, writer)) {
		std::string_view v(vec.data(), vec.size());
		return std::string(v);
	}
	return std::nullopt;
}

bool SshConnection::connect(std::string const &host, int port, bool passwd, const std::string &uid, const std::string &pwd)
{
	Quissh::AuthVar auth;
	if (passwd) {
		auth = Quissh::PasswdAuth({ uid, pwd });
	} else {
		auth = Quissh::PubkeyAuth();
	}
	if (m->quissh.open(host.c_str(), port, auth)) {
		return true;
	}
	qWarning() << "Failed to connect to SSH server.";
	if (m->quissh.is_connected()) {
		m->quissh.close();
	}
	m->sftp.reset();
	return false;
}

void SshConnection::disconnect()
{
	close_sftp();
	m->quissh.close();
}

bool SshConnection::open_sftp()
{
	close_sftp();

	m->sftp = std::make_shared<Quissh::SFTP>(m->quissh);
	if (m->sftp->open()) {
		return true;
	} else {
		qWarning() << "Failed to open SFTP session.";
		m->quissh.close();
		m->sftp.reset();
		return false;
	}

}

void SshConnection::close_sftp()
{
	if (m->sftp) {
		m->sftp->close();
	}
	m->sftp.reset();

}

bool SshConnection::is_sftp_connected() const
{
	return m->quissh.is_connected() && m->quissh.is_sftp_connected();
}

bool SshConnection::unlink(const char *path)
{
	if (!is_sftp_connected()) {
		qWarning() << "Not connected to SFTP server.";
		return false;
	}
	if (m->sftp->unlink(path)) {
		return true;
	} else {
		qWarning() << "Failed to remove file:" << QString::fromStdString(path);
		return false;
	}
}

void SshConnection::add_allowed_command(const std::string &command)
{
	m->quissh.add_allowed_command(command);
}

std::optional<std::vector<SshConnection::FileItem> > SshConnection::list(char const *path)
{
	if (!is_sftp_connected()) {
		qWarning() << "Not connected to SFTP server.";
		return {};
	}
	auto list = m->sftp->ls(path);
	if (list) {
		std::vector<FileItem> files;
		for (const Quissh::FileAttribute &a : *list) {
			files.emplace_back(a.name, a.isdir());
		}
		sort(&files);
		return files;
	}
	return std::nullopt;
}

std::optional<std::string> SshConnection::pull(char const *path)
{
	if (!is_sftp_connected()) {
		qWarning() << "Not connected to SFTP server.";
		return {};
	}
	std::vector<char> out;
	auto Writer = [&](char const *ptr, int len) {
		out.insert(out.end(), ptr, ptr + len);
		return len;
	};
	if (m->sftp->pull(path, Writer)) {
		std::string_view sv(out.data(), out.size());
		return std::string(sv);
	}
	return std::nullopt;
}

void SshConnection::sort(std::vector<FileItem> *files)
{
	std::sort(files->begin(), files->end(), [](SshConnection::FileItem const &a, SshConnection::FileItem const &b){
		auto Compare = [](SshConnection::FileItem const &a, SshConnection::FileItem const &b){
			if (a.isdir || b.isdir) {
				if (!b.isdir) return -1;
				if (!a.isdir) return 1;
			}
			if (a.name[0] == '.' || b.name[0] == '.') {
				if (b.name[0] != '.') return -1;
				if (a.name[0] != '.') return 1;
			}
			return x_stricmp(a.name.c_str(), b.name.c_str());
		};
		return Compare(a, b) < 0;
	});
}
