#include "BlameWindow.h"
#include "ui_BlameWindow.h"
#include "common/misc.h"
#include "CommitPropertyDialog.h"
#include "Git.h"
#include "MainWindow.h"

#include <QMenu>
#include <QToolTip>

enum {
	CommidIdRole = Qt::UserRole,
};

namespace {
struct CommitInfo {
	QString datetime;
	QString author;
	QString email;
	QString message;
};
}

struct BlameWindow::Private {
	QList<BlameItem> list;
	std::map<QString, CommitInfo> commit_cache;
};

BlameWindow::BlameWindow(MainWindow *parent, const QString &filename, const QList<BlameItem> &list)
	: QDialog(parent)
	, ui(new Ui::BlameWindow)
	, m(new Private)
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

	m->list = list;

	int rows = 0;
	for (BlameItem const &item : m->list) {
		if (rows < item.line_number) {
			rows = item.line_number;
		}
	}
	int n = m->list.size();
	if (rows < n) rows = n;

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
	for (BlameItem const &blame: m->list) {
		QString id;
		if (Git::isValidID(blame.commit_id)) {
			id = blame.commit_id.mid(0, 8);
		}

		int col = 0;

		auto NewItem = [](QString const &text){
			return new QTableWidgetItem(text);
		};

		auto SetItem = [&](QTableWidgetItem *item){
			ui->tableWidget->setItem(row, col, item);
			col++;
		};

		QTableWidgetItem *item;

		item = NewItem(id);
		item->setData(CommidIdRole, blame.commit_id);
		SetItem(item);

		item = NewItem(misc::makeDateTimeString(blame.time));
		SetItem(item);

		item = NewItem(QString::number(blame.line_number));
		item->setTextAlignment(Qt::AlignRight);
		SetItem(item);

		item = NewItem(blame.text);
		SetItem(item);

		ui->tableWidget->setRowHeight(row, 24);
		row++;
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->stretchLastSection();

	ui->tableWidget->selectRow(0);
}

BlameWindow::~BlameWindow()
{
	delete m;
	delete ui;
}

MainWindow *BlameWindow::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
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

	if (ptr + 18 < end && memcmp(ptr, "Not Committed Yet ", 18) == 0) {
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

QString BlameWindow::getCommitId(QTableWidgetItem *item) const
{
	return item ? item->data(CommidIdRole).toString() : QString();
}

QString BlameWindow::currentCommitId() const
{
	QString id;
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < m->list.size()) {
		QTableWidgetItem *item = ui->tableWidget->item(row, 0);
		id = getCommitId(item);
	}
	return id;
}

void BlameWindow::on_tableWidget_itemDoubleClicked(QTableWidgetItem *)
{
	QString id = currentCommitId();
	if (Git::isValidID(id)) {
		CommitPropertyDialog dlg(this, mainwindow(), id);
		dlg.showCheckoutButton(false);
		dlg.showJumpButton(true);
		if (dlg.exec() == QDialog::Accepted) {
			close();
		}
	}
}

void BlameWindow::on_tableWidget_customContextMenuRequested(const QPoint &pos)
{
	(void)pos;
	int row = ui->tableWidget->currentRow();
	if (row < 0 || row >= m->list.size()) return;

	Git::CommitItem commit;
	BlameItem blame = m->list[row];
	GitPtr g = mainwindow()->git();
	if (!g->queryCommit(blame.commit_id, &commit)) return;

	QMenu menu;
	QAction *a_property = mainwindow()->addMenuActionProperty(&menu);
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_property) {
			mainwindow()->execCommitPropertyDialog(this, &commit);
			return;
		}
	}
}

void BlameWindow::on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
	(void)current;
	(void)previous;
	QString id = currentCommitId();
	CommitInfo info;
	if (Git::isValidID(id)) {
		auto it = m->commit_cache.find(id);
		if (it != m->commit_cache.end()) {
			info = it->second;
		} else {
			GitPtr g = mainwindow()->git();
			Git::CommitItem commit;
			if (g->queryCommit(id, &commit)) {
				info.datetime = misc::makeDateTimeString(commit.commit_date);
				info.author = commit.author;
				info.email = commit.email;
				info.message = commit.message;
			}
		}
	} else {
		id = QString();
	}
	QString author = info.author;
	if (!info.email.isEmpty()) {
		author = author + " <" + info.email + '>';
	}
	ui->lineEdit_commit_id->setText(id);
	ui->lineEdit_date->setText(info.datetime);
	ui->lineEdit_author->setText(author);
	ui->lineEdit_message->setText(info.message);
}
