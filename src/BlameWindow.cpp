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

BlameWindow::BlameWindow(MainWindow *parent, QString const &filename, const QList<BlameItem> &list)
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
		auto *item = new QTableWidgetItem(cols[col]);
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
		item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
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

QList<BlameItem> BlameWindow::parseBlame(std::string_view const &str)
{
	QList<BlameItem> list;
	std::vector<std::string> lines;
	misc::splitLines(str, &lines, false);
	BlameItem item;
	for (std::string const &line : lines) {
		if (line[0] == '\t') {
			item.text = QString::fromUtf8(line.c_str() + 1);
			list.push_back(item);
			item.commit_id = QString();
			item.text = QString();
		} else {
			char const *p = line.c_str();
			char const *q = strchr(p, ' ');
			if (q) {
				QString label = QString::fromLatin1(p, int(q - p));
				if (item.commit_id.isEmpty()) {
					item.commit_id = label;
					int a, b, c;
					if (sscanf(q + 1, "%d %d %d", &a, &b, &c) >= 2) {
						item.line_number = b;
					}
				} else {
					auto Value = [&](){
						return QString::fromLatin1(q + 1);
					};
					if (label == "author") {
						item.author = Value();
					} else if (label == "author-time") {
						qint64 t = Value().toLong();
						item.time = QDateTime::fromMSecsSinceEpoch(t * 1000);
					}
				}
			}
		}
	}
	return list;
}

Git::CommitID BlameWindow::getCommitId(QTableWidgetItem *item) const
{
	return item ? Git::CommitID(item->data(CommidIdRole).toString()) : Git::CommitID();
}

Git::CommitID BlameWindow::currentCommitId() const
{
	Git::CommitID id;
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < m->list.size()) {
		QTableWidgetItem *item = ui->tableWidget->item(row, 0);
		id = getCommitId(item);
	}
	return id;
}

void BlameWindow::on_tableWidget_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitID commit_id = currentCommitId();
	if (Git::isValidID(commit_id)) {
		Git::CommitItem commit_item = mainwindow()->commitItem(nullptr, commit_id);
		CommitPropertyDialog dlg(this, mainwindow(), commit_item);
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

	BlameItem blame = m->list[row];
	GitPtr g = mainwindow()->git();
	std::optional<Git::CommitItem> commit = g->queryCommitItem(Git::CommitID(blame.commit_id));
	if (!commit) return;

	QMenu menu;
	QAction *a_property = mainwindow()->addMenuActionProperty(&menu);
	QAction *a = menu.exec(QCursor::pos() + QPoint(8, -8));
	if (a) {
		if (a == a_property) {
			mainwindow()->execCommitPropertyDialog(this, *commit);
			return;
		}
	}
}

void BlameWindow::on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
	(void)current;
	(void)previous;
	Git::CommitID commit_id = currentCommitId();
	CommitInfo info;
	if (Git::isValidID(commit_id)) {
		auto it = m->commit_cache.find(commit_id.toQString());
		if (it != m->commit_cache.end()) {
			info = it->second;
		} else {
			GitPtr g = mainwindow()->git();
			auto commit = g->queryCommitItem(commit_id);
			if (commit) {
				info.datetime = misc::makeDateTimeString(commit->commit_date);
				info.author = commit->author;
				info.email = commit->email;
				info.message = commit->message;
				m->commit_cache[commit_id.toQString()] = info;
			}
		}
	} else {
		commit_id = {};
	}
	QString author = info.author;
	if (!info.email.isEmpty()) {
		author = author + " <" + info.email + '>';
	}
	ui->lineEdit_commit_id->setText(commit_id.toQString());
	ui->lineEdit_date->setText(info.datetime);
	ui->lineEdit_author->setText(author);
	ui->lineEdit_message->setText(info.message);
}
