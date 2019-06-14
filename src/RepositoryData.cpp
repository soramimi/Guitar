#include "RepositoryData.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <vector>

class TagState {
private:
	std::vector<ushort> arr;
public:
	void push(QString const &tag)
	{
		if (arr.empty()) {
			arr.reserve(100);
		}
		size_t s = arr.size();
		size_t t = tag.size();
		arr.resize(s + t + 1);
		arr[s] = '/';
		std::copy_n(tag.utf16(), t, &arr[s + 1]);
	}
	void push(QStringRef const &tag)
	{
		push(tag.toString());
	}
	void pop()
	{
		size_t s = arr.size();
		while (s > 0) {
			s--;
			if (arr[s] == '/') break;
		}
		arr.resize(s);
	}
	bool is(char const *path) const
	{
		size_t s = arr.size();
		for (size_t i = 0; i < s; i++) {
			if (arr[i] != path[i]) return false;
		}
		return path[s] == 0;
	}
	bool operator == (char const *path) const
	{
		return is(path);
	}
	QString str() const
	{
		return arr.empty() ? QString() : QString::fromUtf16(&arr[0], arr.size());
	}
};

bool RepositoryBookmark::save(QString const &path, QList<RepositoryItem> const *items)
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		QXmlStreamWriter writer(&file);
		writer.setAutoFormatting(true);
		writer.writeStartDocument();
		writer.writeStartElement("repositories");
		for (RepositoryItem const &item : *items) {
			if (item.name.isEmpty() && item.local_dir.isEmpty()) {
				continue;
			}
			writer.writeStartElement("repository");
			writer.writeAttribute("name", item.name);
			writer.writeAttribute("group", item.group);
			if (!item.local_dir.isEmpty()) {
				QString local = item.local_dir;
				local.replace('\\', '/');
				writer.writeStartElement("local");
				writer.writeCharacters(local);
				writer.writeEndElement(); // local
			}
			writer.writeEndElement(); // repository
		}
		writer.writeEndElement(); // repositories
		writer.writeEndDocument();
		return true;
	}
	return false;
}

QList<RepositoryItem> RepositoryBookmark::load(QString const &path)
{
	QList<RepositoryItem> items;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		RepositoryItem item;
		QString text;
		TagState state;
		QXmlStreamReader reader(&file);
		reader.setNamespaceProcessing(false);
		while (!reader.atEnd()) {
			QXmlStreamReader::TokenType tt = reader.readNext();
			switch (tt) {
			case QXmlStreamReader::StartElement:
				state.push(reader.name());
				{
					QXmlStreamAttributes atts = reader.attributes();
					if (state == "/repositories/repository") {
						item = RepositoryItem();
						item.name = atts.value("name").toString();
						item.group = atts.value("group").toString();
					} else if (state == "/repositories/repository/local") {
						text = QString();
					}
				}
				break;
			case QXmlStreamReader::EndElement:
				if (state == "/repositories/repository/local") {
					item.local_dir = text;
					item.local_dir.replace('\\', '/');
				} else if (state == "/repositories/repository") {
					items.push_back(item);
				}
				state.pop();
				break;
			case QXmlStreamReader::Characters:
				text.append(reader.text());
				break;
			}
		}
	}
	return items;
}
