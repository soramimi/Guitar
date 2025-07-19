#ifndef SSHDIALOG_H
#define SSHDIALOG_H
#ifdef UNSAFE_ENABLED

#include <QDialog>

namespace Ui {
class Dialog;
}

class QListWidgetItem;

class SshConnection {
public:
	struct FileItem {
		std::string name;
		bool isdir = false;
		FileItem() = default;
		FileItem(std::string const &n, bool d = false)
			: name(n)
			, isdir(d)
		{
		}
	};
private:
	struct Private;
	Private *m;

	SshConnection(SshConnection const &) = delete;
	SshConnection &operator=(SshConnection const &) = delete;
	SshConnection(SshConnection &&) = delete;
	SshConnection &operator=(SshConnection &&) = delete;
public:
	SshConnection();
	~SshConnection();
	bool connect(const std::string &host, int port, bool passwd, const std::string &uid = {}, const std::string &pwd = {});
	void disconnect();
	bool is_sftp_connected() const;
	void add_allowed_command(const std::string &command);
	std::optional<std::string> exec(char const *command);
	std::optional<std::vector<FileItem>> list(const char *path);
	std::optional<std::string> pull(char const *path);
	static void sort(std::vector<FileItem> *files);
	bool open_sftp();
	void close_sftp();
	bool open_channel();
	void close_channel();
	bool unlink(char const *path);
};

class SshDialog : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	Ui::Dialog *ui;

	void onSelect();
	void updateFiles();
	void updateFileList();
public:
	explicit SshDialog(QWidget *parent, SshConnection *ssh);
	~SshDialog();

	enum AuthMethod {
		PublicKey,
		Password
	};

	void setHostName(QString const &host);
	void setPort(int port);
	void setAuthMethod(AuthMethod method);
	void setUserName(QString const &uid);
	void setPassword(QString const &pwd);

	QString hostName() const;
	int port() const;
	AuthMethod authMethod() const;
	QString userName() const;
	QString password() const;

	void updateUI();
	void doConnect();
	void doDisconnect();
	bool isSftpConnected() const;

	QString selectedPath() const;
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);
	void on_pushButton_connect_clicked();
	void on_pushButton_disconnect_clicked();
	void on_radioButton_key_clicked();
	void on_radioButton_pwd_clicked();
	void on_pushButton_ok_clicked();
protected:
	void keyPressEvent(QKeyEvent *event);

	// QDialog interface
public slots:
	int exec();
};

#endif
#endif // SSHDIALOG_H
