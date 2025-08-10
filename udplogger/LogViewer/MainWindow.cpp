#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "../RemoteLogger.h"
#include <future>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
typedef int ssize_t;
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#define ioctlsocket ioctl
#endif

RemoteLogger logger;
void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	std::string message = qFormatLogMessage(type, context, msg).toStdString();
	logger.send(message, context.file, context.line);
}

struct MainWindow::Private {
	std::future<int> thread;
	int sockfd = -1;
	std::mutex mutex;
	std::vector<LogItem> log_items;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

	connect(this, &MainWindow::logReady, this, &MainWindow::onLogReady);

	startReceiveThread();

	if (!logger.open()) {
		qDebug() << "Failed to open socket";
		return;
	}
	installCustomLogger();
}

MainWindow::~MainWindow()
{
	closesocket();
	m->thread.get(); // wait for thread to finish
	delete m;
	delete ui;
}

void MainWindow::installCustomLogger()
{
	qInstallMessageHandler(customLogHandler);
}

void MainWindow::closesocket()
{
	int fd = -1;
	std::swap(fd, m->sockfd);
	if (fd != -1) {
#ifdef _WIN32
		::closesocket(fd);
#else
		::close(fd);
#endif
	}
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	logger.close();
	QMainWindow::closeEvent(event);
}

void MainWindow::startReceiveThread()
{
	m->thread = std::async(std::launch::async, [this]()->int{

		constexpr int BUFFER_SIZE = 1500;

		// ソケットの作成
		if ((m->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			qDebug() << "socket creation failed";
			return -1;
		}

		// サーバーアドレスの設定
		struct sockaddr_in server_addr;
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(RemoteLogger::PORT);

		// ソケットにアドレスをバインド
		if (bind(m->sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
			qDebug() << "bind failed";
			closesocket();
			return -2;
		}

		// ノンブロッキングモードに設定
		{
#ifdef _WIN32
			u_long val = 1;
#else
			int val = 1;
#endif
			ioctlsocket(m->sockfd, FIONBIO, &val);
		}

		// データ受信ループ
		while (m->sockfd != -1) {
			char buffer[BUFFER_SIZE];
			struct sockaddr_in client_addr;
			socklen_t addr_len = sizeof(client_addr);
			ssize_t n = recvfrom(m->sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
			std::string from = inet_ntoa(client_addr.sin_addr);
			if (n > 0) {
				auto item = LogItem::parse(std::string_view(buffer, n));
				item.from = QString::fromStdString(from);
				{
					std::lock_guard lock(m->mutex);
					m->log_items.push_back(item);
				}
				emit logReady();
			} else {
#ifdef _WIN32
				Sleep(1);
#else
				usleep(1000);
#endif
			}
		}

		closesocket();
		return 0;
	});
}

void MainWindow::onLogReady()
{
	std::vector<LogItem> items;
	{
		std::lock_guard lock(m->mutex);
		items.insert(items.end(), m->log_items.begin(), m->log_items.end());
		m->log_items.clear();
	}
	ui->tableView->add(std::move(items));
}

void MainWindow::on_action_clear_triggered()
{
	ui->tableView->clear();
}

void MainWindow::on_action_send_test_message_triggered()
{
#if 0
	logger.send("This is a test message!");
#else
	qDebug() << "Test message from qDebug";
#endif
}


