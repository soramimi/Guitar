#include "gpg.h"

#include <QFileInfo>
#include <QList>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "Process.h"

#include "common/misc.h"

void gpg::parse(char const *begin, char const *end, QList<gpg::Data> *keys)
{
	char const *ptr = begin;
	char const *line = ptr;
	std::string pub, uid, sub;
	std::vector<uint8_t> fingerprint;
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
					p = strchr(p, '/');
					if (p) {
						p++;
						char id[100];
						if (sscanf(p, "%99s %u-%u-%u", id, &key->year, &key->month, &key->day) == 4) {
							key->id = id;
						}
					}
				};
				auto ParseUID = [](char const *p, gpg::Data *key){
					auto Search = [](char const *p, char c)->char const *{
						while (*p) {
							if (*p == c) return p;
							p++;
						}
						return nullptr;
					};
					char const *q1 = nullptr;
					if (*p == '[') {
						q1 = Search(p, ']');
						if (q1) {
							p = q1 + 1;
							while (isspace((unsigned char)*p)) p++;
						}
					}
					char const *q2 = Search(p, '(');
					char const *q3 = Search(p, ')');
					char const *q4 = Search(p, '<');
					if (!q2) {
						q2 = q3 = q4;
					}
					char const *e = q2 ? q2 : (p + strlen(p));
					while (p < e && isspace((unsigned char)e[-1])) e--;
					key->name = QString::fromUtf8(p, int(e - p));
					if (q2 && q3 && q2 < q3 && *q2 == '(') {
						q2++;
						key->comment = QString::fromUtf8(q2, int(q3 - q2));
					}
					if (q4) {
						q4++;
						char const *q5 = strchr(q4, '>');
						if (q5) {
							key->mail = QString::fromUtf8(q4, int(q5 - q4));
						}
					}
				};
				if (!pub.empty() && !uid.empty()) {
					gpg::Data key;
					ParseKey(SkipHeader(pub.c_str()), &key);
					ParseUID(SkipHeader(uid.c_str()), &key);
					key.pub = QString::fromStdString(pub);
					key.sub = QString::fromStdString(sub);
					key.fingerprint = fingerprint;
					keys->push_back(key);
				}
				pub = uid = sub = std::string();
				fingerprint.clear();
			} else if (line < ptr) {
				std::string s(line, ptr - line);
				if (strncmp(s.c_str(), "pub ", 4) == 0) {
					pub = misc::trimmed(s.substr(4));
				} else if (strncmp(s.c_str(), "uid ", 4) == 0) {
					uid = misc::trimmed(s.substr(4));
				} else if (strncmp(s.c_str(), "sub ", 4) == 0) {
					sub = misc::trimmed(s.substr(4));
				} else if (!pub.empty() && uid.empty()) {
					char const *p = strchr(s.c_str(), '=');
					if (p) {
						fingerprint = misc::hex_string_to_bin({p + 1}, " ");
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

bool gpg::listKeys(QString const &gpg_command, QList<gpg::Data> *keys)
{
	keys->clear();

	QString cmd = gpg_command;
	if (!QFileInfo(cmd).isExecutable()) return false;

#if 1
	cmd = "\"%1\" --list-keys --keyid-format LONG --fingerprint";
	cmd = cmd.arg(gpg_command);

	Process proc;
	proc.start(cmd, false);
	proc.wait();
	if (!proc.outbytes.empty()) {
		char const *begin = &proc.outbytes[0];
		char const *end = begin + proc.outbytes.size();
		parse(begin, end, keys);
	}
#else
	QFile file("/home/soramimi/a/keys.txt");
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readAll();
		if (!ba.isEmpty()) {
			char const *begin = ba.data();
			char const *end = begin + ba.size();
			gpg::parse(begin, end, keys);
			qDebug() << keys->size();
		}
	}
#endif
	return true;
}

//QList<gpg::Item> gpg::load(QByteArray const &json)
//{
//	QList<Item> items;

//	QJsonDocument doc = QJsonDocument::fromJson(json);
//	QJsonArray a1 = doc.array();
//	for (QJsonValue const &v1: a1) {
//		QJsonObject o1 = v1.toObject(QJsonObject());

//		Item item;

//		QJsonObject o2 = o1["pub"].toObject();
//		item.pub.id = o2["key"].toString();
//		QString timestamp = o2["timestamp"].toString();
//		item.pub.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);

//		QJsonArray a2 = o1["sub"].toArray();
//		for (QJsonValue const &v2 : a2) {
//			QJsonObject o3 = v2.toObject(QJsonObject());
//			Key sub;
//			sub.id = o3["key"].toString();
//			QString timestamp = o3["timestamp"].toString();
//			sub.timestamp = QDateTime::fromString(timestamp, Qt::ISODate);
//			item.sub.push_back(sub);
//		}

//		QJsonArray a3 = o1["uid"].toArray();
//		for (QJsonValue const &v3 : a3) {
//			QJsonObject o3 = v3.toObject(QJsonObject());
//			UID uid;
//			uid.name = o3["name"].toString();
//			uid.email = o3["email"].toString();
//			uid.comment = o3["comment"].toString();
//			item.uid.push_back(uid);
//		}

//		items.push_back(item);
//	}

//	return items;
//}
