#include <BasicProcessPosix.h>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#ifndef _WIN32

namespace {

std::string read_available(ProcessPosixPty *process)
{
	std::string output;
	char buffer[4096];
	while (true) {
		int const size = process->read_output(buffer, sizeof(buffer));
		if (size <= 0) break;
		output.append(buffer, static_cast<size_t>(size));
	}
	return output;
}

std::string shell_command(std::string const &script)
{
	return "\"/bin/sh\" \"-c\" \"" + script + "\"";
}

}

TEST(PtyProcess, ProvidesTerminalSizeAnsiAndFinalOutput)
{
	ProcessPosixPty process;
	process.set_capture_output_vector(false);
	process.resize_terminal(91, 37);
	std::string environment = "TERM=xterm-256color";
	environment.push_back('\0');
	environment += "COLORTERM=truecolor";
	process.start(shell_command(
		"if test -t 0 && test -t 1 && test -t 2; then printf TTY; else printf NOTTY; fi; "
		"printf '\\n'; stty size; printf '%s/%s\\n' $TERM $COLORTERM; "
		"printf '\\033[31mANSI\\033[0m'; printf STDERR >&2; printf TAIL"),
		environment, true);

	ProcessResult const result = process.wait(5000);
	ASSERT_FALSE(result.running());
	EXPECT_EQ(result.exit_code(), 0U);
	std::string const output = read_available(&process);
	EXPECT_NE(output.find("TTY\r\n"), std::string::npos);
	EXPECT_NE(output.find("37 91\r\n"), std::string::npos);
	EXPECT_NE(output.find("xterm-256color/truecolor\r\n"), std::string::npos);
	EXPECT_NE(output.find("\x1b[31mANSI\x1b[0mSTDERRTAIL"), std::string::npos);
}

TEST(PtyProcess, ResizeNotifiesForegroundProcessGroup)
{
	ProcessPosixPty process;
	process.set_capture_output_vector(false);
	process.resize_terminal(80, 25);
	process.start(shell_command(
		"trap 'stty size; printf RESIZED; exit 0' WINCH; printf READY; while :; do sleep 1; done"),
		"TERM=xterm-256color", true);

	std::string output;
	auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (output.find("READY") == std::string::npos
		&& std::chrono::steady_clock::now() < deadline) {
		output += read_available(&process);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_NE(output.find("READY"), std::string::npos);

	process.resize_terminal(132, 43);
	ProcessResult result = process.wait(5000);
	if (result.running()) {
		process.stop();
		result = process.wait(1000);
	}
	output += read_available(&process);
	EXPECT_FALSE(result.running());
	EXPECT_EQ(result.exit_code(), 0U);
	EXPECT_NE(output.find("43 132\r\nRESIZED"), std::string::npos);
}

#endif
