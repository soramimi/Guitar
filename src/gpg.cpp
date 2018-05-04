#include "gpg.h"

#include <QFileInfo>
#include <QList>
#include "MyProcess.h"

bool gpg::listKeys(const QString &gpg_command, QList<gpg::Key> *keys)
{
	keys->clear();

	QString cmd = gpg_command;
	if (!QFileInfo(cmd).isExecutable()) return false;

//	cmd = "\"%1\" --list-keys";
	cmd = "\"%1\" --fingerprint";
	cmd = cmd.arg(gpg_command);

	Process proc;
	proc.start(cmd, false);
	proc.wait();
	if (!proc.outbytes.empty()) {
		char const *begin = &proc.outbytes[0];
		char const *end = begin + proc.outbytes.size();
		char const *ptr = begin;
		char const *line = ptr;
		std::string pub, uid, sub;
		QByteArray fingerprint;
		while (1) {
			int c = 0;
			if (ptr < end) {
				c = *ptr & 0xff;
			}
			if (c == '\r' || c == '\n' || c == 0) {
				if (ptr == line || c == 0) {
					auto SkipHeader = [](char const *p){
						while (isalpha((unsigned char)*p)) p++;
						while (isspace((unsigned char)*p)) p++;
						return p;
					};
					auto ParseKey = [](char const *p, gpg::Key *key){
						char id[100];
						if (sscanf(p, "%u%c/%99s %u-%u-%u", &key->len, &key->type, id, &key->year, &key->month, &key->day) == 6) {
							key->id = id;
						}

					};
					auto ParseUID = [](char const *p, gpg::Key *key){
						char const *q1 = strrchr(p, '(');
						char const *q2 = strrchr(p, ')');
						char const *q3 = strrchr(p, '<');
						if (!q1) {
							q1 = q2 = q3;
						}
						char const *e = q1 ? q1 : (p + strlen(p));
						while (p < e && isspace((unsigned char)e[-1])) e--;
						key->name = QString::fromUtf8(p, e - p);
						if (q1 && q2 && q1 < q2 && *q1 == '(') {
							q1++;
							key->comment = QString::fromUtf8(q1, q2 - q1);
						}
						if (q3) {
							q3++;
							char const *q4 = strchr(q3, '>');
							if (q4) {
								key->mail = QString::fromUtf8(q3, q4 - q3);
							}
						}
					};
					if (!pub.empty() && !uid.empty()) {
						gpg::Key key;
						ParseKey(SkipHeader(pub.c_str()), &key);
						ParseUID(SkipHeader(uid.c_str()), &key);
						key.fingerprint = fingerprint;
						keys->push_back(key);
					}
					pub = uid = sub = std::string();
					fingerprint.clear();
				} else if (line < ptr) {
					std::string s(line, ptr - line);
					if (strncmp(s.c_str(), "pub ", 4) == 0) {
						pub = s;
					} else if (strncmp(s.c_str(), "uid ", 4) == 0) {
						uid = s;
					} else if (strncmp(s.c_str(), "sub ", 4) == 0) {
						sub = s;
					} else if (!pub.empty() && uid.empty()) {
						char const *p = strchr(s.c_str(), '=');
						if (p) {
							p++;
							fingerprint.clear();
							while (p[0] && p[1]) {
								if (isxdigit(p[0] & 0xff) && isxdigit(p[1] & 0xff)) {
									char tmp[3];
									tmp[0] = p[0];
									tmp[1] = p[1];
									tmp[2] = 0;
									int v = strtol(tmp, nullptr, 16);
									fingerprint.push_back(v);
									p += 2;
								} else {
									p++;
								}
							}
						}
					}
				}
				if (c == 0) {
					break;
				}
				ptr++;
				if (c == '\r') {
					if (ptr < end && *ptr == '\n') {
						ptr++;
					}
				}
				line = ptr;
			} else {
				ptr++;
			}
		}
	}
	return true;
}
