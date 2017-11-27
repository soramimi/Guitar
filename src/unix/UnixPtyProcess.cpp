#include "UnixPtyProcess.h"

void UnixPtyProcess::parseArgs(const QString &cmd, QStringList *out)
{
	out->clear();
	ushort const *begin = cmd.utf16();
	ushort const *end = begin + cmd.size();
	std::vector<ushort> tmp;
	ushort const *ptr = begin;
	ushort quote = 0;
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = *ptr;
		}
		if (c == '\"' && ptr + 2 < end && ptr[1] == '\"' && ptr[2] == '\"') {
			tmp.push_back(c);
			ptr += 3;
		} else {
			if (quote != 0 && c != 0) {
				if (c == quote) {
					quote = 0;
				} else {
					tmp.push_back(c);
				}
			} else if (c == '\"') {
				quote = c;
			} else if (isspace(c) || c == 0) {
				if (!tmp.empty()) {
					QString s = QString::fromUtf16(&tmp[0], tmp.size());
					out->push_back(s);
				}
				if (c == 0) break;
				tmp.clear();
			} else {
				tmp.push_back(c);
			}
			ptr++;
		}
	}
}

int UnixPtyProcess::start(const QString &program)
{
	QStringList argv;
	QStringList env;
	parseArgs(program, &argv);
	Pty::start(argv[0], argv, env, 0, 0);
	waitForStarted();
}
