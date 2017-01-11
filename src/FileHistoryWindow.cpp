#include "FileHistoryWindow.h"
#include "MainWindow.h"
#include "ui_FileHistoryWindow.h"
#include "misc.h"
#include "GitDiff.h"
#include "joinpath.h"

enum {
	DiffIndexRole = Qt::UserRole,
};

FileHistoryWindow::FileHistoryWindow(QWidget *parent, GitPtr g, const QString &path)
	: QDialog(parent)
	, ui(new Ui::FileHistoryWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	mainwindow = qobject_cast<MainWindow *>(parent);
	Q_ASSERT(mainwindow);

	ui->widget_diff_pixmap->imbue_(mainwindow, &data_);
	ui->widget_diff_left->imbue_(&data_);
	ui->widget_diff_right->imbue_(&data_);

	ui->splitter->setSizes({100, 100});

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_pixmap, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged2(int)));
	connect(ui->widget_diff_left, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_left, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_right, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_right, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));

	Q_ASSERT(g);
	Q_ASSERT(g->isValidWorkingCopy());
	this->g = g;
	this->path = path;

	int top_margin = 1;
	int bottom_margin = 1;
	ui->widget_diff_left->updateDrawData_(top_margin, bottom_margin);


	collectFileHistory();

	updateDiffView();
}

FileHistoryWindow::~FileHistoryWindow()
{
	delete ui;
}

void FileHistoryWindow::collectFileHistory()
{
	commit_item_list = g->log_all(path, mainwindow->limitLogCount(), mainwindow->limitLogTime());

	QStringList cols = {
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Description"),
	};
	int n = cols.size();
	ui->tableWidget_log->setColumnCount(n);
	ui->tableWidget_log->setRowCount(0);
	for (int i = 0; i < n; i++) {
		QString const &text = cols[i];
		QTableWidgetItem *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}

	int count = commit_item_list.size();
	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const &commit = commit_item_list[row];
		int col = 0;
		auto AddColumn = [&](QString const &text){
			QTableWidgetItem *item = new QTableWidgetItem(text);
//			item->setSizeHint(QSize(100, 20));
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};

		QString commit_id = mainwindow->abbrevCommitID(commit);
		QString datetime = misc::makeDateTimeString(commit.commit_date);
		AddColumn(commit_id);
		AddColumn(datetime);
		AddColumn(commit.author);
		AddColumn(commit.message);
		ui->tableWidget_log->setRowHeight(row, 24);
	}

	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	ui->tableWidget_log->setCurrentCell(0, 0);
}

void FileHistoryWindow::clearDiffView()
{
	ui->widget_diff_pixmap->clear(false);
	ui->widget_diff_left->clear(ViewType::Left);
	ui->widget_diff_right->clear(ViewType::Right);
}

int FileHistoryWindow::fileviewHeight() const
{
	return ui->widget_diff_left->height();
}

QString FileHistoryWindow::formatLine(const QString &text, bool diffmode)
{
	if (text.isEmpty()) return text;
	std::vector<ushort> vec;
	vec.reserve(text.size() + 100);
	ushort const *begin = text.utf16();
	ushort const *end = begin + text.size();
	ushort const *ptr = begin;
	if (diffmode) {
		vec.push_back(*ptr);
		ptr++;
	}
	int x = 0;
	while (ptr < end) {
		if (*ptr == '\t') {
			do {
				vec.push_back(' ');
				x++;
			} while ((x % 4) != 0);
			ptr++;
		} else {
			vec.push_back(*ptr);
			ptr++;
			x++;
		}
	}
	return QString::fromUtf16(&vec[0], vec.size());
}


void FileHistoryWindow::updateVerticalScrollBar()
{
	QScrollBar *sb = ui->verticalScrollBar;
	if (mainwindow->drawdata()->line_height > 0) {
		int lines_per_widget = fileviewHeight() / mainwindow->drawdata()->line_height;
		if (lines_per_widget < diffdata()->left_lines.size() + 1) {
			sb->setRange(0, diffdata()->left_lines.size() - lines_per_widget + 1);
			sb->setPageStep(lines_per_widget);
			return;
		}
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

void FileHistoryWindow::setDiffText_(QList<TextDiffLine> const &left, QList<TextDiffLine> const &right, bool diffmode)
{
	enum Pane {
		Left,
		Right,
	};
	auto DO = [&](QList<TextDiffLine> const &lines, Pane pane, QList<TextDiffLine> *out){
		out->clear();
		int linenum = 0;
		for (TextDiffLine const &line : lines) {
			TextDiffLine item = line;
			item.type = TextDiffLine::Unknown;
			if (diffmode) {
				ushort c = item.line.utf16()[0];
				if (c == ' ') {
					item.type = TextDiffLine::Unchanged;
					item.line_number = linenum++;
				} else if (c == '+') {
					item.type = TextDiffLine::Add;
					if (pane == Right) {
						item.line_number = linenum++;
					}
				} else if (c == '-') {
					item.type = TextDiffLine::Del;
					if (pane == Left) {
						item.line_number = linenum++;
					}
				}
			}
			item.line = formatLine(item.line, diffmode);
			out->push_back(item);
		}
	};
	DO(left, Left, &diffdata()->left_lines);
	DO(right, Right, &diffdata()->right_lines);

	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileHistoryWindow::init_diff_data_(Git::Diff const &diff)
{
	clearDiffView();
	diffdata()->path = diff.path;
	diffdata()->left = diff.blob.a;
	diffdata()->right = diff.blob.b;
}

void FileHistoryWindow::setTextDiffData(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	init_diff_data_(diff);

	if (uncommited) {
		QString path = workingdir / diff.path;
		diffdata()->right.id = GitDiff::prependPathPrefix(path);
	}

	if (ba.isEmpty()) {
		diffdata()->original_lines.clear();
	} else {
		diffdata()->original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<TextDiffLine> left_newlines;
	QList<TextDiffLine> right_newlines;

	size_t linenum = diffdata()->original_lines.size();

	std::vector<HunkItem> hunks;
	int number = 0;
	for (auto it = diff.hunks.begin(); it != diff.hunks.end(); it++, number++) {
		QString at = it->at;
		if (at.startsWith("@@ -")) {
			size_t pos = 0;
			size_t len = 0;
			ushort const *p = at.utf16() + 4;
			auto ParseNumber = [&](){
				size_t v = 0;
				while (QChar::isDigit(*p)) {
					v = v * 10 + (*p - '0');
					p++;
				}
				return v;
			};
			pos = ParseNumber();
			if (*p == ',') {
				p++;
				len = ParseNumber();
			} else {
				len = 1;
			}
			if (pos > 0) pos--;
			HunkItem item;
			item.hunk_number = number;
			item.pos = pos;
			item.len = len;
			for (QString const &line : it->lines) {
				item.lines.push_back(line);
			}
			hunks.push_back(item);
		}
	}
	std::sort(hunks.begin(), hunks.end(), [](HunkItem const &l, HunkItem const &r){
		return l.pos + l.len < r.pos + r.len;
	});
	size_t h = hunks.size();
	while (linenum > 0 || h > 0) {
		while (h > 0) {
			int hunk_number = h - 1;
			HunkItem const &hi = hunks[hunk_number];
			if (hi.pos + hi.len < linenum) {
				break;
			}
			std::vector<TextDiffLine> tmp_left;
			std::vector<TextDiffLine> tmp_right;
			int minus = 0;
			int plus = 0;
			auto FlushBlank = [&](){
				while (minus < plus) {
					tmp_left.push_back(QString());
					minus++;
				}
				while (minus > plus) {
					tmp_right.push_back(QString());
					plus++;
				}
				minus = plus = 0;
			};
			for (auto it = hi.lines.begin(); it != hi.lines.end(); it++) {
				QString line = *it;
				ushort c = line.utf16()[0];
				if (c == '-') {
					minus++;
					TextDiffLine l(line);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
				} else if (c == '+') {
					plus++;
					TextDiffLine l(line);
					l.hunk_number = hunk_number;
					tmp_right.push_back(l);
				} else {
					FlushBlank();
					TextDiffLine l(line);
					l.hunk_number = hunk_number;
					tmp_left.push_back(l);
					tmp_right.push_back(l);
				}
			}
			FlushBlank();
			for (auto it = tmp_left.rbegin(); it != tmp_left.rend(); it++) left_newlines.push_back(*it);
			for (auto it = tmp_right.rbegin(); it != tmp_right.rend(); it++) right_newlines.push_back(*it);
			linenum = hi.pos;
			h--;
		}
		if (linenum > 0) {
			linenum--;
			if (linenum < (size_t)diffdata()->original_lines.size()) {
				QString line = ' ' + diffdata()->original_lines[linenum];
				left_newlines.push_back(line);
				right_newlines.push_back(line);
			}
		}
	}

	std::reverse(left_newlines.begin(), left_newlines.end());
	std::reverse(right_newlines.begin(), right_newlines.end());
	setDiffText_(left_newlines, right_newlines, true);
}



void FileHistoryWindow::updateDiffView()
{
	clearDiffView();

	int row = ui->tableWidget_log->currentRow();
	if (row >= 0 && row + 1 < commit_item_list.size()) {
		Git::CommitItem const &commit_left = commit_item_list[row + 1]; // older
		Git::CommitItem const &commit_right = commit_item_list[row];    // newer

		QString id_left = GitDiff().findFileID(g, commit_left.commit_id, path);
		QString id_right = GitDiff().findFileID(g, commit_right.commit_id, path);



		Git::Diff diff;
		diff.path = path;
		diff.blob.a.id = id_left;
		diff.blob.b.id = id_right;
		diff.mode = "0";
		QString text = GitDiff::diffFile(g, diff.blob.a.id, diff.blob.b.id);
		GitDiff::parseDiff(text, &diff, &diff);

		QByteArray ba;
		mainwindow->cat_file(g, diff.blob.a.id, &ba);
		setTextDiffData(ba, diff, false, g->workingRepositoryDir());

		ui->verticalScrollBar->setValue(0);
		updateVerticalScrollBar();
		ui->widget_diff_pixmap->clear(false);
//		updateSliderCursor();
//		ui->widget_diff_pixmap->clear(true);
		updateSliderCursor();
		ui->widget_diff_pixmap->update();
	}
}

void FileHistoryWindow::updateSliderCursor()
{
	int total = totalTextLines();
	int value = fileviewScrollPos();
	int size = visibleLines();
	ui->widget_diff_pixmap->setScrollPos(total, value, size);
}


void FileHistoryWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	updateDiffView();
}

void FileHistoryWindow::scrollTo(int value)
{
	drawdata()->scrollpos = value;
	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void FileHistoryWindow::onScrollValueChanged(int value)
{
	scrollTo(value);

	updateSliderCursor();
}

void FileHistoryWindow::onDiffWidgetWheelScroll(int lines)
{
	while (lines > 0) {
		ui->verticalScrollBar->triggerAction(QScrollBar::SliderSingleStepAdd);
		lines--;
	}
	while (lines < 0) {
		ui->verticalScrollBar->triggerAction(QScrollBar::SliderSingleStepSub);
		lines++;
	}
}

void FileHistoryWindow::onScrollValueChanged2(int value)
{
	ui->verticalScrollBar->setValue(value);
}

void FileHistoryWindow::onDiffWidgetResized()
{
	updateSliderCursor();
	updateVerticalScrollBar();
}
