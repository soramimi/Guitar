#ifndef REMOTELOGGER_H
#define REMOTELOGGER_H

#include <string>

class RemoteLogger {
public:
	static constexpr int PORT = 1024;
private:
	struct Private;
	Private *m;
public:
	RemoteLogger();
	~RemoteLogger();
	bool open(const char *remote = nullptr, int port = 1024);
	void close();
	void send(std::string message, char const *file = nullptr, int line = 0);
	static void initialize();
	static void cleanup();
};

#endif // REMOTELOGGER_H
