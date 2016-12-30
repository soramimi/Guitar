#include "RepositoryData.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFile>
#include <vector>

RepositoryBookmark::RepositoryBookmark()
{

}

bool RepositoryBookmark::save(const QString &path, QList<RepositoryItem> const *items)
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
				writer.writeStartElement("local");
				writer.writeCharacters(item.local_dir);
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

QList<RepositoryItem> RepositoryBookmark::load(const QString &path)
{
	QList<RepositoryItem> items;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		enum State {
			STATE_UNKNOWN,
			STATE_ROOT,
			STATE_REPOSITORIES,
			STATE_REPOSITORIES_REPOSITORY,
			STATE_REPOSITORIES_REPOSITORY_LOCAL,
		};
		RepositoryItem item;
		QString text;
		std::vector<State> state_stack;
		QXmlStreamReader reader(&file);
		reader.setNamespaceProcessing(false);
		while (!reader.atEnd()) {
			State state = STATE_UNKNOWN;
			State newstate = STATE_UNKNOWN;
			if (!state_stack.empty()) {
				state = state_stack.back();
			}
			QXmlStreamReader::TokenType tt = reader.readNext();
			switch (tt) {
			case QXmlStreamReader::StartElement:
				if (state_stack.empty()) {
					if (reader.name() == "repositories") {
						newstate = STATE_REPOSITORIES;
					}
				} else {
					QXmlStreamAttributes atts = reader.attributes();
					if (state == STATE_REPOSITORIES && reader.name() == "repository") {
						newstate = STATE_REPOSITORIES_REPOSITORY;
						item = RepositoryItem();
						item.name = atts.value("name").toString();
						item.group = atts.value("group").toString();
					} else if (state == STATE_REPOSITORIES_REPOSITORY && reader.name() == "local") {
						newstate = STATE_REPOSITORIES_REPOSITORY_LOCAL;
						text = QString();
					}
				}
				state_stack.push_back(newstate);
				break;
			case QXmlStreamReader::EndElement:
				if (state == STATE_REPOSITORIES_REPOSITORY_LOCAL) {
						item.local_dir = text;
				} else if (state == STATE_REPOSITORIES_REPOSITORY) {
					items.push_back(item);
				}
				if (!state_stack.empty()) {
					state_stack.pop_back();
				}
				break;
			case QXmlStreamReader::Characters:
				text.append(reader.text());
				break;
			}
		}
	}
	return items;
}
