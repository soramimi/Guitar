#include "MyProcess.h"



int misc2::runCommand(QString const &cmd, QByteArray const *in, QByteArray *out)
{
	Process proc;
	proc.start(cmd, (bool)in);

	if (in) {
		int n = in->size();
		if (n > 0) {
			if (n > 65536) n = 65536;
			proc.writeInput(in->data(), n);
		}
		proc.closeInput(false);
	}

	int r = proc.wait();
	if (proc.outbytes.empty()) {
		out->clear();
	} else {
		out->append(&proc.outbytes[0], proc.outbytes.size());
	}
	return r;
}

int misc2::runCommand(QString const &cmd, QByteArray *out)
{
	return runCommand(cmd, nullptr, out);
}

