#include "RepositoryData.h"
#include "XmlTagState.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <QStringRef>

bool RepositoryBookmark::save(QString const &path, QList<RepositoryData> const *items)
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		QXmlStreamWriter writer(&file);
		writer.setAutoFormatting(true);
		writer.writeStartDocument();
		writer.writeStartElement("repositories");
		for (RepositoryData const &item : *items) {
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
			if (!item.ssh_key.isEmpty()) {
				writer.writeStartElement("sshkey");
				writer.writeCharacters(item.ssh_key);
				writer.writeEndElement(); // sshkey
			}
			writer.writeEndElement(); // repository
		}
		writer.writeEndElement(); // repositories
		writer.writeEndDocument();
		return true;
	}
	return false;
}

QList<RepositoryData> RepositoryBookmark::load(QString const &path)
{
	QList<RepositoryData> items;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		RepositoryData item;
		QString text;
		XmlTagState state;
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
						item = RepositoryData();
						item.name = atts.value("name").toString();
						item.group = atts.value("group").toString();
					} else if (state == "/repositories/repository/local") {
						text = QString();
					} else if (state == "/repositories/repository/sshkey") {
						text = QString();
					}
				}
				break;
			case QXmlStreamReader::EndElement:
				if (state == "/repositories/repository/local") {
					item.local_dir = text;
					item.local_dir.replace('\\', '/');
				} else if (state == "/repositories/repository/sshkey") {
					item.ssh_key = text;
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
