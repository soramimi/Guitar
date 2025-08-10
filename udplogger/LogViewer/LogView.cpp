#include "LogView.h"
#include <QAbstractItemModel>
#include <QHeaderView>
#include <vector>
#include <QScrollBar>
#include <string_view>
#include "../../src/common/strformat.h"
#include "../../src/common/htmlencode.h"
#include <toi.h>

LogItem LogItem::parse(const std::string_view &sv)
{
	LogItem ret;

	auto Set = [&](std::string_view const &key, std::string_view const &value){
		if (key.empty()) return;
		if (key == "pid") {
			ret.pid = toi<int>(value);
		} else if (key == "d") {
			int y, m, d;
			if (sscanf(std::string(value).c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
				ret.ts.setDate(QDate(y, m, d));
			}
		} else if (key == "t") {
			int h, m, s;
			if (sscanf(std::string(value).c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
				ret.ts.setTime(QTime(h, m, s));
			}
		} else if (key == "us") {
			if (value.size() == 6) {
				int us = 0;
				for (int i = 0; i < 6; i++) {
					us *= 10;
					us += value[i] - '0';
				}
				ret.us = us;
			} else {
				ret.us = -1;

			}
		} else if (key == "m") {
			std::string s = html_decode(value);
			ret.message = QString::fromStdString(s).trimmed();
		} else if (key == "f") {
			std::string s = html_decode(value);
			ret.file = QString::fromStdString(s).trimmed();
		} else if (key == "l") {
			std::string s = html_decode(value);
			ret.line = toi<int>(s);
		}
	};

	char const *begin = sv.data();
	char const *end = begin + sv.size();
	char const *p = begin;
	char const *k = begin;
	char const *v = begin;
	while (1) {
		int c = 0;
		if (p < end) {
			c = (unsigned char)*p;
		}
		if (c == '<' || c == 0) {
			if (k < v) {
				std::string_view key(k, v - k);
				v++;
				std::string_view value(v, p - v);
				Set(key, value);
			}
			if (c == 0) break;
			p++;
			k = v = p;
		} else if (c == '>') {
			v = p;
			p++;
		} else {
			p++;
		}
	}

	if (ret.message.isEmpty()) {
		ret = {};
		ret.message = QString::fromUtf8(sv.data(), sv.size());
	}

	return ret;
}

class MyModel : public QAbstractItemModel {
public:
	std::vector<LogItem> items_;

	MyModel()
	{
	}

	LogItem const &item(int row) const
	{
		return items_[row];
	}
	LogItem const &item(QModelIndex const &index) const
	{
		return item(index.row());
	}
	QModelIndex index(int row, int column, const QModelIndex &parent = {}) const
	{
		return createIndex(row, column);
	}
	QModelIndex parent(const QModelIndex &child) const
	{
		return {};
	}
	int rowCount(const QModelIndex &parent = {}) const
	{
		return (int)items_.size();
	}
	enum Column {
		C_DATE,
		C_TIME,
		C_FROM,
		C_PID,
		C_SOURCE,
		C_MESSAGE,
		_C_COUNT
	};
	int columnCount(const QModelIndex &parent = {}) const
	{
		return _C_COUNT;
	}
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const
	{
		if (orientation == Qt::Horizontal) {
			if (role == Qt::DisplayRole) {
				switch (section) {
				case C_DATE:     return tr("Date");
				case C_TIME:     return tr("Time");
				case C_FROM:     return tr("From");
				case C_PID:      return tr("PID");
				case C_SOURCE:   return tr("Source");
				case C_MESSAGE:  return tr("Message");
				}
			}
		}
		return {};
	}
	QVariant data(const QModelIndex &index, int role) const
	{
		if (role == Qt::DisplayRole) {
			auto const &t = item(index);
			if (index.column() == C_DATE) {
				if (t.ts.date().isValid()) {
					return QString::asprintf("%d-%02d-%02d", t.ts.date().year(), t.ts.date().month(), t.ts.date().day());
				}
			} else if (index.column() == C_TIME) {
				if (t.ts.date().isValid()) {
					QString s = QString::asprintf("%02d:%02d:%02d", t.ts.time().hour(), t.ts.time().minute(), t.ts.time().second());
					if (t.us >= 0) {
						s += QString::asprintf(".%06d", t.us);
					}
					return s;
				}
			} else if (index.column() == C_FROM) {
				return t.from;
			} else if (index.column() == C_PID) {
					return QString::asprintf("%d", t.pid);
			} else if (index.column() == C_SOURCE) {
				QString s = t.file;
				if (!s.isEmpty()) {
					s.replace('\\', '/');
					auto i = s.lastIndexOf('/');
					if (i >= 0) {
						s = s.mid(i + 1);
					}
					if (t.line > 0) {
						s += QString::asprintf(" (%d)", t.line);
					}
				}
				return s;
			} else if (index.column() == C_MESSAGE) {
				return t.message;
			}
		}
		return {};
	}
	void add(std::vector<LogItem> &&item)
	{
		beginResetModel();
		items_.insert(items_.end(), std::make_move_iterator(item.begin()), std::make_move_iterator(item.end()));
		endResetModel();
	}
	void clear()
	{
		beginResetModel();
		items_.clear();
		endResetModel();
	}
};

struct LogView::Private {
	MyModel model;
};

LogView::LogView(QWidget *parent)
	: QTableView{parent}
	, m(new Private)
{
	setModel(&m->model);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	verticalHeader()->setVisible(false);
	horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	horizontalHeader()->setStretchLastSection(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

LogView::~LogView()
{
	delete m;
}

void LogView::add(std::vector<LogItem> &&item)
{
	int i = currentIndex().row();

	m->model.add(std::move(item));

	horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

	if (i + 2 == m->model.rowCount()) {
		setCurrentIndex(m->model.index(i + 1, 0));
		auto *sb = verticalScrollBar();
		sb->setValue(sb->maximum());
	} else {
		setCurrentIndex(m->model.index(i, 0));
	}
}

void LogView::clear()
{
	m->model.clear();
}
