#include "gpg.h"

#include <QFileInfo>
#include <QList>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "MyProcess.h"

bool gpg::listKeys(const QString &gpg_command, QList<gpg::Data> *keys)
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
					auto ParseKey = [](char const *p, gpg::Data *key){
						char id[100];
						if (sscanf(p, "%u%c/%99s %u-%u-%u", &key->len, &key->type, id, &key->year, &key->month, &key->day) == 6) {
							key->id = id;
						}

					};
					auto ParseUID = [](char const *p, gpg::Data *key){
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
						gpg::Data key;
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

QList<gpg::Item> gpg::load(const QByteArray &json)
{
	QList<Item> items;

	QJsonDocument doc = QJsonDocument::fromJson(json);
	QJsonArray a1 = doc.array();
	for (QJsonValue const &v1: a1) {
		QJsonObject o1 = v1.toObject(QJsonObject());

		Item item;

		QJsonObject o2 = o1["pub"].toObject();
		item.pub.id = o2["key"].toString();
		QString timestamp = o2["timestamp"].toString();
		item.pub.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);

		QJsonArray a2 = o1["sub"].toArray();
		for (QJsonValue const &v2 : a2) {
			QJsonObject o3 = v2.toObject(QJsonObject());
			Key sub;
			sub.id = o3["key"].toString();
			QString timestamp = o3["timestamp"].toString();
			sub.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);
			item.sub.push_back(sub);
		}

		QJsonArray a3 = o1["uid"].toArray();
		for (QJsonValue const &v3 : a3) {
			QJsonObject o3 = v3.toObject(QJsonObject());
			UID uid;
			uid.name = o3["name"].toString();
			uid.email = o3["email"].toString();
			uid.comment = o3["comment"].toString();
			item.uid.push_back(uid);
		}

		items.push_back(item);
	}

	return items;
}
