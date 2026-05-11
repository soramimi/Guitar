#ifndef PROCESSWIN_H
#define PROCESSWIN_H

#include "AbstractProcess.h"

// #define USE_WINPTY

class ProcessWin : public AbstractProcess {
private:
	struct Private;
	Private *m;
	void writeOutput(char const *buf, size_t len);
	bool exec_win(const std::string &cmd, bool use_input);
public:
	ProcessWin();
	~ProcessWin();
	std::string outstring() const;
	std::string errstring() const;
	void start(const std::string &command, bool use_input);
	int wait();
	void writeInput(const char *ptr, int len);
	void closeInput(bool justnow);
	void readResult(std::vector<char> *out);
	void stop();
	int getExitCode() const;

	bool isRunning() const;
	const std::vector<char> &stdout_bytes() const;
	const std::vector<char> &stderr_bytes() const;
};

#if 0
class ProcessWinPty : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;

	std::string exec_winpty(const std::string &cmd, const std::string &env, bool use_input);
public:
	ProcessWinPty();
	~ProcessWinPty();
	bool isRunning() const;
	void writeInput(const char *ptr, int len);
	void start(const std::string &cmd, std::string const &env, bool use_input);
	bool wait(unsigned long time = ULONG_MAX);
	void stop();
	int getExitCode() const;
	void readResult(std::vector<char> *out);
	void closeInput();

	int readOutputStreaming(char *ptr, int len) override;
};
#endif

class ProcessWinConPTY : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;
	bool exec_win_conpty(const std::string &cmd, const std::string &env, bool use_input);
public:
	ProcessWinConPTY();
	~ProcessWinConPTY();
	void closeInput();
	bool isRunning() const override;
	void writeInput(const char *ptr, int len) override;
	void start(const std::string &cmd, std::string const &env, bool use_input) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const override;
	virtual int readOutputStreaming(char *ptr, int len);
};

#endif // PROCESSWIN_H
