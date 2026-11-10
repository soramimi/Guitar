#pragma once

#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace process::command {
inline void validate(std::vector<std::string> const &args)
{
	if (args.empty() || args.front().empty()) throw std::invalid_argument("empty executable");
	for (auto const &arg : args) {
		if (arg.find('\0') != std::string::npos) throw std::invalid_argument("NUL in process argument");
	}
}

// Compatibility grammar used by historical Guitar command builders. Not shell syntax.
inline std::string quote(std::string const &arg)
{
	if (arg.find('\0') != std::string::npos) throw std::invalid_argument("NUL in process argument");
	std::string out = "\"";
	for (char c : arg) out += c == '"' ? "\"\"\"" : std::string(1, c);
	return out + '"';
}
inline std::vector<std::string> split(std::string const &line)
{
	if (line.find('\0') != std::string::npos) throw std::invalid_argument("NUL in command line");
	std::vector<std::string> args;
	std::string arg;
	bool quoted = false, present = false;
	for (size_t i = 0; i < line.size();) {
		char c = line[i];
		if (c == '"' && line.compare(i, 3, "\"\"\"") == 0) {
			arg += '"'; present = true; i += 3;
		} else if (c == '"') {
			quoted = !quoted; present = true; ++i;
		} else if (!quoted && std::isspace(static_cast<unsigned char>(c))) {
			if (present) args.push_back(arg);
			arg.clear(); present = false; ++i;
		} else {
			arg += c; present = true; ++i;
		}
	}
	if (quoted) throw std::invalid_argument("unterminated argument quote");
	if (present) args.push_back(arg);
	return args;
}
inline std::string windowsLine(std::vector<std::string> const &args)
{
	validate(args);
	std::string line;
	for (auto const &arg : args) {
		if (!line.empty()) line += ' ';
		line += '"';
		size_t slashes = 0;
		for (char c : arg) {
			if (c == '\\') { ++slashes; continue; }
			line.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
			slashes = 0;
			line += c;
		}
		line.append(slashes * 2, '\\');
		line += '"';
	}
	return line;
}
inline std::string nativeLine(std::vector<std::string> const &args)
{
	validate(args);
#ifdef _WIN32
	return windowsLine(args);
#else
	std::string line;
	for (auto const &arg : args) {
		if (!line.empty()) line += ' ';
		line += quote(arg);
	}
	return line;
#endif
}
// Git executes core.sshCommand through its POSIX shell, including Git for Windows.
inline std::string shellQuote(std::string const &arg)
{
	if (arg.find('\0') != std::string::npos) throw std::invalid_argument("NUL in shell argument");
	std::string out = "'";
	for (char c : arg) out += c == '\'' ? "'\\''" : std::string(1, c);
	return out + '\'';
}
} // namespace process::command
