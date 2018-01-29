#include "BlameWindow.h"
#include "ui_BlameWindow.h"
#include "common/misc.h"

BlameWindow::BlameWindow(QWidget *parent, const QString &filename, const QList<BlameItem> &list) :
	QDialog(parent),
	ui(new Ui::BlameWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	{
		QString s = "Blame : %1";
		s = s.arg(filename);
		setWindowTitle(s);
	}

	list_ = list;

	int rows = 0;
	for (BlameItem const &item : list_) {
		if (rows < item.line_number) {
			rows = item.line_number;
		}
	}

	QStringList cols = {
		"Commit",
		"Time",
		"Line",
		"Text",
	};

	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(rows);

	for (int col = 0; col < cols.size(); col++) {
		QTableWidgetItem *item = new QTableWidgetItem(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}

	int row = 0;
	for (BlameItem const &blame: list_) {
		QTableWidgetItem *item;
		int col = 0;
		QString id = blame.commit_id.mid(0, 8);
		item = new QTableWidgetItem(id);
		ui->tableWidget->setItem(row, col++, item);
		item = new QTableWidgetItem(misc::makeDateTimeString(blame.time));
		ui->tableWidget->setItem(row, col++, item);
		item = new QTableWidgetItem(QString::number(blame.line_number));
		item->setTextAlignment(Qt::AlignRight);
		ui->tableWidget->setItem(row, col++, item);
		item = new QTableWidgetItem(blame.text);
		ui->tableWidget->setItem(row, col++, item);
		ui->tableWidget->setRowHeight(row, 24);
		row++;
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->stretchLastSection();
}

BlameWindow::~BlameWindow()
{
	delete ui;
}

namespace {

bool parseBlameHeader(const char *ptr, const char *end, BlameItem *out)
{
	*out = BlameItem();

	auto SkipSpace = [&](){
		while (ptr < end && isspace(*ptr & 0xff)) {
			ptr++;
		}
	};

	auto Token = [&](){
		char const *head = ptr;
		while (ptr < end) {
			int c = *ptr & 0xff;
			if (isspace(c)) break;
			ptr++;
		}
		return QString::fromLatin1(head, ptr - head);
	};

	BlameItem item;

	item.commit_id = Token();
	if (item.commit_id.isEmpty()) return false;

	SkipSpace();

	if (ptr < end && *ptr == '(') {
		ptr++;
	} else {
		return false;
	}

	if (ptr + 18 < end && memcmp(ptr, "Not Committed Yet ", 18)) {
		ptr += 18;
	} else {
		item.author = Token();
		SkipSpace();
	}

	if (ptr + 25 < end && ptr[4] == '-' && ptr[7] == '-' && ptr[10] == ' ' && ptr[13] == ':' && ptr[16] == ':') {
		int year = atoi(ptr);
		int month = atoi(ptr + 5);
		int day = atoi(ptr + 8);
		int hour = atoi(ptr + 11);
		int minute = atoi(ptr + 14);
		int second = atoi(ptr + 17);
		item.time = QDateTime(QDate(year, month, day), QTime(hour, minute, second));
		ptr += 25;
	} else {
		return false;
	}

	SkipSpace();
	item.line_number = atoi(ptr);

	*out = item;
	return true;
}

QString textWithoutTab(const char *ptr, const char *end)
{
	std::vector<char> vec;
	vec.reserve(end - ptr);
	int x = 0;
	while (ptr < end) {
		int c = *ptr & 0xff;
		if (c == '\t') {
			do {
				vec.push_back(' ');
				x++;
			} while (x % 4 != 0);
		} else if (c < ' ') {
			vec.push_back('.');
			x++;
		} else {
			vec.push_back(c);
			x++;
		}
		ptr++;
	}
	if (vec.empty()) return QString();
	return QString::fromUtf8(&vec[0], vec.size());
}

} // end namespace

QList<BlameItem> BlameWindow::parseBlame(const char *begin, const char *end)
{
	QList<BlameItem> list;
	char const *ptr = begin;
	while (ptr < end) {
		char const *mid = nullptr;
		char const *eol = ptr;
		while (eol < end) {
			if (!mid && eol + 1 < end && eol[0] == ')' && eol[1] == ' ') {
				mid = eol + 2;
			}
			if (*eol == '\r' || *eol == '\n') {
				break;
			}
			eol++;
		}
		if (mid) {
			BlameItem t;
			parseBlameHeader(ptr, mid, &t);
			t.text = textWithoutTab(mid, eol);
			list.push_back(t);
		}
		ptr = eol;
		if (ptr < end) {
			if (*ptr == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
			} else if (*ptr == '\n') {
				ptr++;
			}
		}
	}
	return list;
}
