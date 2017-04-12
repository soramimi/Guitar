#ifndef LOCALSOCKETREADER_H
#define LOCALSOCKETREADER_H

#include <QLocalSocket>

class LocalSocketReader : public QObject {
	Q_OBJECT
private:
	QMutex mutex;
	QLocalSocket *sock;
	QByteArray data;
	static void destroy(LocalSocketReader *p)
	{
		delete p;
	}
public:
	LocalSocketReader(QLocalSocket *sock);
	QByteArray takeData();

public slots:
	void onReadyRead();
	void onReadChannelFinished();

signals:
	void readyRead(LocalSocketReader *p);
	void readChannelFinished(LocalSocketReader *p);
};

#endif // LOCALSOCKETREADER_H
