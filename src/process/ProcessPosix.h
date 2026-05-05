#ifndef PROCESSPOSIX_H
#define PROCESSPOSIX_H

#include "AbstractProcess.h"
#include "MyProcess2.h"

class ProcessPosix : public AbstractProcess {
	using ProcessResult = process::ProcessResult;
private:
	struct Private;
	Private *m;

	void writeStdOut(char const *buf, size_t len);
	void writeStdErr(char const *buf, size_t len);

	std::optional<ProcessResult> run(const std::string &cmd, bool use_input);
private:
	ProcessPosix(ProcessPosix &&) = delete;
	ProcessPosix(const ProcessPosix &) = delete;
	ProcessPosix & operator = (ProcessPosix &&) = delete;
	ProcessPosix & operator = (const ProcessPosix &) = delete;
public:
	ProcessPosix();
	~ProcessPosix();
	bool isRunning() const;
	void writeInput(const char *ptr, int len) override;
	int readOutput(char *ptr, int len);
	void start(const std::string &command, bool use_input) override;
	int wait() override;
	void stop() override;
	int getExitCode() const override;
	void readResult(std::vector<char> *out);

	std::vector<char> const &stdout_bytes() const;
	std::vector<char> const &stderr_bytes() const;

	void closeInput(bool justnow);
};

class ProcessPosixPty : public AbstractPtyProcess {
private:
	struct Private;
	Private *m;

	void writeStdOut(char const *buf, size_t len);
	void writeStdErr(char const *buf, size_t len);

	bool exec_posixpty(std::string const &cmd);
private:
	ProcessPosixPty(ProcessPosixPty &&) = delete;
	ProcessPosixPty(const ProcessPosixPty &) = delete;
	ProcessPosixPty & operator = (ProcessPosixPty &&) = delete;
	ProcessPosixPty & operator = (const ProcessPosixPty &) = delete;
public:
	ProcessPosixPty();
	~ProcessPosixPty();
	bool isRunning() const;
	void writeInput(const char *ptr, int len) override;
	void closeInput();
	int readOutputStreaming(char *ptr, int len);
	void start(const std::string &cmd, const std::string &env, bool use_input) override;
	bool wait(unsigned long time = ULONG_MAX) override;
	void stop() override;
	int getExitCode() const;
	void readResult(std::vector<char> *out);

	std::string outstring() const;
	std::string errstring() const;
};


#endif // PROCESSPOSIX_H

