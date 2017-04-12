#include "LocalSocketReader.h"

LocalSocketReader::LocalSocketReader(QLocalSocket *sock)
	: sock(sock)
{
}

QByteArray LocalSocketReader::takeData()
{
	QByteArray ba;
	mutex.lock();
	std::swap(ba, data);
	mutex.unlock();
	return ba;
}

void LocalSocketReader::onReadyRead()
{
	QByteArray ba = sock->readAll();
	mutex.lock();
	data.append(ba);
	mutex.unlock();
	emit readyRead(this);
}

void LocalSocketReader::onReadChannelFinished()
{
	emit readChannelFinished(this);
}



