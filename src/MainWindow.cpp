#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AboutDialog.h"
#include "CheckoutBranchDialog.h"
#include "CloneDialog.h"
#include "CommitPropertyDialog.h"
#include "ConfigCredentialHelperDialog.h"
#include "FileUtil.h"
#include "Git.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "LibGit2.h"
#include "main.h"
#include "MergeBranchDialog.h"
#include "misc.h"
#include "MySettings.h"
#include "NewBranchDialog.h"
#include "Terminal.h"
#include "PushDialog.h"
#include "RepositoryData.h"
#include "SelectGitCommandDialog.h"
#include "SettingsDialog.h"
#include "TextEditDialog.h"
#include "RepositoryPropertyDialog.h"
#include "EditTagDialog.h"
#include "DeleteTagsDialog.h"
#include <deque>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStandardPaths>
#include <QKeyEvent>
#include <set>
#include <QProcess>
#include <QDirIterator>




DiffWidgetData::DrawData::DrawData()
{
	bgcolor_text = QColor(255, 255, 255);
	bgcolor_gray = QColor(224, 224, 224);
	bgcolor_add = QColor(192, 240, 192);
	bgcolor_del = QColor(255, 224, 224);
	bgcolor_add_dark = QColor(64, 192, 64);
	bgcolor_del_dark = QColor(240, 64, 64);
}

enum {
	IndexRole = Qt::UserRole,
	FilePathRole,
	DiffIndexRole,
	HunkIndexRole,
};

enum TreeItem {
	GroupItem = -1,
};

static inline bool isGroupItem(QTreeWidgetItem *item)
{
	if (item) {
		int index = item->data(0, IndexRole).toInt();
		if (index == GroupItem) {
			return true;
		}
	}
	return false;
}

static inline QString getFilePath(QListWidgetItem *item)
{
	if (!item) return QString();
	return item->data(FilePathRole).toString();
}

static inline int indexOfDiff(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(DiffIndexRole).toInt();
}

static inline int getHunkIndex(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(HunkIndexRole).toInt();
}

struct MainWindow::Private {
	Git::Context gcx;
	ApplicationSettings appsettings;
	QList<RepositoryItem> repos;
	RepositoryItem current_repo;
	Git::Branch current_branch;
	QList<Git::Diff> diffs;
	QStringList added;
	Git::CommitItemList logs;
	bool uncommited_changes = false;
	int timer_interval_ms;
	int update_files_list_counter;
	int interval_250ms_counter;
	QImage graph_color;
	std::map<QString, QList<Git::Branch>> branch_map;
	std::map<QString, QList<Git::Tag>> tag_map;
	QString repository_filter_text;
	QPixmap digits;
	QIcon repository_icon;
	QIcon folder_icon;
	DiffWidgetData diff_widget_data;
	unsigned int temp_file_counter = 0;
};

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	pv = new Private();
	ui->setupUi(this);
	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	ui->treeWidget_repos->installEventFilter(this);

	showFileList(true);

	pv->digits.load(":/image/digits.png");
	pv->graph_color.load(":/image/graphcolor.png");

	pv->repository_icon = QIcon(":/image/repository.png");
	pv->folder_icon = QIcon(":/image/folder.png");

	prepareLogTableWidget();

#ifdef Q_OS_WIN
	{
		QFont font;

		font = ui->label_repo_name->font();
		font.setFamily("Meiryo");
		ui->label_repo_name->setFont(font);

		font = ui->label_branch_name->font();
		font.setFamily("Meiryo");
		ui->label_branch_name->setFont(font);
	}
#endif

	SettingsDialog::loadSettings(&pv->appsettings);
	{
		MySettings s;
		s.beginGroup("Global");
		pv->gcx.git_command = s.value("GitCommand").toString();
		s.endGroup();
	}

#if USE_LIBGIT2
	LibGit2::init();
//	LibGit2::test();
#endif

	connect(ui->verticalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged(int)));
	connect(ui->widget_diff_left, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_left, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_right, SIGNAL(scrollByWheel(int)), this, SLOT(onDiffWidgetWheelScroll(int)));
	connect(ui->widget_diff_right, SIGNAL(resized()), this, SLOT(onDiffWidgetResized()));
	connect(ui->widget_diff_pixmap, SIGNAL(valueChanged(int)), this, SLOT(onScrollValueChanged2(int)));
	connect(ui->treeWidget_repos, SIGNAL(dropped()), this, SLOT(onRepositoriesTreeDropped()));;

	QString path = getBookmarksFilePath();
	pv->repos = RepositoryBookmark::load(path);
	updateRepositoriesList();

	pv->timer_interval_ms = 20;
	pv->update_files_list_counter = 0;
	pv->interval_250ms_counter = 0;
	startTimer(pv->timer_interval_ms);
}

MainWindow::~MainWindow()
{
#if USE_LIBGIT2
	LibGit2::shutdown();
#endif
	deleteTempFiles();
	delete pv;
	delete ui;
}



bool MainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &pv->repos);
}

void MainWindow::saveRepositoryBookmark(RepositoryItem item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	bool done = false;
	for (int i = 0; i < pv->repos.size(); i++) {
		RepositoryItem *p = &pv->repos[i];
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		pv->repos.push_back(item);
	}
	saveRepositoryBookmarks();
	updateRepositoriesList();
}

void MainWindow::buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryItem> *repos)
{
	QString name = item->text(0);
	if (isGroupItem(item)) {
		int n = item->childCount();
		for (int i = 0; i < n; i++) {
			QTreeWidgetItem *child = item->child(i);
			QString sub = group / name;
			buildRepoTree(sub, child, repos);
		}
	} else {
		RepositoryItem const *repo = repositoryItem(item);
		if (repo) {
			RepositoryItem newrepo = *repo;
			newrepo.name = name;
			newrepo.group = group;
			item->setData(0, IndexRole, repos->size());
			repos->push_back(newrepo);
		}
	}
}

void MainWindow::refrectRepositories()
{
	QList<RepositoryItem> newrepos;
	int n = ui->treeWidget_repos->topLevelItemCount();
	for (int i = 0; i < n; i++) {
		QTreeWidgetItem *item = ui->treeWidget_repos->topLevelItem(i);
		buildRepoTree(QString(), item, &newrepos);
	}
	pv->repos = std::move(newrepos);
	saveRepositoryBookmarks();
}

void MainWindow::onRepositoriesTreeDropped()
{
	refrectRepositories();
	QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
	if (item) item->setExpanded(true);
}

const QPixmap &MainWindow::digitsPixmap() const
{
	return pv->digits;
}

int MainWindow::digitWidth() const
{
	return 5;
}

int MainWindow::digitHeight() const
{
	return 7;
}

void MainWindow::drawDigit(QPainter *pr, int x, int y, int n) const
{
	int w = digitWidth();
	int h = digitHeight();
	pr->drawPixmap(x, y, w, h, pv->digits, n * w, 0, w, h);
}

QString MainWindow::defaultWorkingDir() const
{
	return pv->appsettings.default_working_dir;
}

QColor MainWindow::color(unsigned int i)
{
	unsigned int n = pv->graph_color.width();
	if (n > 0) {
		n--;
		if (i > n) i = n;
		QRgb const *p = (QRgb const *)pv->graph_color.scanLine(0);
		return QColor(qRed(p[i]), qGreen(p[i]), qBlue(p[i]));
	}
	return Qt::black;
}

bool MainWindow::isThereUncommitedChanges() const
{
	return pv->uncommited_changes;
}


Git::CommitItemList const *MainWindow::logs() const
{
	return &pv->logs;
}

QString MainWindow::currentWorkingCopyDir() const
{
	return pv->current_repo.local_dir;
}

GitPtr MainWindow::git(QString const &dir)
{
	return dir.isEmpty() ? std::shared_ptr<Git>() : std::shared_ptr<Git>(new Git(pv->gcx, dir));
}

GitPtr MainWindow::git()
{
	return git(currentWorkingCopyDir());
}

QString MainWindow::makeRepositoryName(QString const &loc)
{
	int i = loc.lastIndexOf('/');
	int j = loc.lastIndexOf('\\');
	if (i < j) i = j;
	if (i >= 0) {
		i++;
		j = loc.size();
		if (loc.endsWith(".git")) {
			j -= 4;
		}
		return loc.mid(i, j - i);
	}
	return QString();
}

void MainWindow::updateSliderCursor()
{
	int total = totalTextLines();
	int value = fileviewScrollPos();
	int size = visibleLines();
	ui->widget_diff_pixmap->setScrollPos(total, value, size);
}

void MainWindow::onScrollValueChanged(int value)
{
	scrollTo(value);

	updateSliderCursor();
}

void MainWindow::onScrollValueChanged2(int value)
{
	ui->verticalScrollBar->setValue(value);
}

int MainWindow::repositoryIndex_(QTreeWidgetItem *item)
{
	if (item) {
		int i = item->data(0, IndexRole).toInt();
		if (i >= 0 && i < pv->repos.size()) {
			return i;
		}
	}
	return -1;
}

RepositoryItem const *MainWindow::repositoryItem(QTreeWidgetItem *item)
{
	int row = repositoryIndex_(item);
	if (row >= 0 && row < pv->repos.size()) {
		return &pv->repos[row];
	}
	return nullptr;
}

static QTreeWidgetItem *newQTreeWidgetItem()
{
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

QTreeWidgetItem *MainWindow::newQTreeWidgetFolderItem(QString const &name)
{
	QTreeWidgetItem *item = newQTreeWidgetItem();
	item->setText(0, name);
	item->setData(0, IndexRole, GroupItem);
	item->setIcon(0, pv->folder_icon);
	return item;
}

void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	pv->repos = RepositoryBookmark::load(path);

	QString filter = pv->repository_filter_text;

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < pv->repos.size(); i++) {
		RepositoryItem const &repo = pv->repos[i];
		if (!filter.isEmpty() && repo.name.indexOf(filter, 0, Qt::CaseInsensitive) < 0) {
			continue;
		}
		QTreeWidgetItem *parent = nullptr;
		{
			QString group = repo.group;
			if (group.startsWith('/')) {
				group = group.mid(1);
			}
			auto it = parentmap.find(group);
			if (it != parentmap.end()) {
				parent = it->second;
			}
			if (!parent) {
				QStringList list = group.split('/', QString::SkipEmptyParts);
				if (list.isEmpty()) {
					list.push_back(tr("Default"));
				}
				for (QString const &name : list) {
					if (name.isEmpty()) continue;
					if (!parent) {
						auto it = parentmap.find(name);
						if (it != parentmap.end()) {
							parent = it->second;
						} else {
							parent = newQTreeWidgetFolderItem(name);
							ui->treeWidget_repos->addTopLevelItem(parent);
						}
					} else {
						QTreeWidgetItem *child = newQTreeWidgetFolderItem(name);
						parent->addChild(child);
						parent = child;
					}
					parent->setExpanded(true);
				}
				Q_ASSERT(parent);
				parentmap[group] = parent;
			}
			parent->setData(0, FilePathRole, "");
		}
		QTreeWidgetItem *child = newQTreeWidgetItem();
		child->setText(0, repo.name);
		child->setData(0, IndexRole, i);
		child->setIcon(0, pv->repository_icon);
		child->setFlags(child->flags() & ~Qt::ItemIsDropEnabled);
		parent->addChild(child);
		parent->setExpanded(true);
	}
}


void MainWindow::updateHeadFilesList(bool single)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	updateFilesList(QString(), "HEAD", single);

	pv->uncommited_changes = !pv->diffs.empty();
}

void MainWindow::showFileList(bool single)
{
	ui->stackedWidget->setCurrentWidget(single ? ui->page_1 : ui->page_2);
}

void MainWindow::clearFileList()
{
	showFileList(true);
	ui->listWidget_unstaged->clear();
	ui->listWidget_staged->clear();
	ui->listWidget_files->clear();
}

void MainWindow::clearLog()
{
	pv->logs.clear();
	pv->uncommited_changes = false;
	ui->tableWidget_log->clearContents();
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::clearRepositoryInfo()
{
	pv->current_branch = Git::Branch();
	ui->label_repo_name->setText(QString());
	ui->label_branch_name->setText(QString());
}

QString MainWindow::diff_(QString const &old_id, QString const &new_id) // obsolete
{
#if 1
	GitPtr g = git();
	if (g.get()) {
		return g->diff(old_id, new_id);
	}
#else
#endif
	return QString();
}

void MainWindow::makeDiff(QString const &old_id, QString const &new_id) // obsolete : diff文字列の中にバイナリファイルがあると、それ以降を処理できない
{
	pv->diffs.clear();

	QString s = diff_(old_id, new_id);

	QStringList lines = misc::splitLines(s);

	enum class ItemType {
		Normal,
		NewFile,
	};
	ItemType itemtype = ItemType::Normal;

	bool f = false;
	for (QString const &line : lines) {
		ushort c = line.utf16()[0];
		if (c == '@') {
			if (line.startsWith("@@ ")) {
				if (!pv->diffs.isEmpty()) {
					pv->diffs.back().hunks.push_back(Git::Hunk());
					pv->diffs.back().hunks.back().at = line;
				}
				f = true;
			}
		} else {
			if (f) {
				if (c == ' ' || c == '-' || c == '+') {
					// nop
				} else {
					f = false;
				}
			}
			if (f) {
				if (!pv->diffs.isEmpty() && !pv->diffs.back().hunks.isEmpty()) {
					pv->diffs.back().hunks.back().lines.push_back(line);
				}
			} else {
				enum class HeaderCode {
					unknown,
					diff,
					index,
					a,
					b,
				};
				HeaderCode h = HeaderCode::unknown;
				if (line.startsWith("diff ")) {
					h = HeaderCode::diff;
				} else if (line.startsWith("index ")) {
					h = HeaderCode::index;
				} else if (line.startsWith("new file ")) {
					itemtype = ItemType::NewFile;
				} else if (line.startsWith("--- ")) {
					h = HeaderCode::a;
				} else if (line.startsWith("+++ ")) {
					h = HeaderCode::b;
				}
				if (h != HeaderCode::unknown) {
					if (pv->diffs.isEmpty() || !pv->diffs.back().hunks.isEmpty()) {
						pv->diffs.push_back(Git::Diff());
					}
					auto GetPath = [](QString s){
						QString prefix;
						if (s.startsWith("--- ")) {
							s = s.mid(4);
							prefix = "a/";
						} else if (s.startsWith("+++ ")) {
							s = s.mid(4);
							prefix = "b/";
						}
						s = Git::trimPath(s);
						if (!prefix.isEmpty() && s.startsWith(prefix)) {
							s = s.mid(prefix.size());
						}
						return s;
					};
					switch (h) {
					case HeaderCode::diff:
						itemtype = ItemType::Normal;
						break;
					case HeaderCode::index:
						pv->diffs.back().index = line;
						if (line.indexOf("..") == 46) {
							if (itemtype == ItemType::NewFile) {
								// nop
							} else {
								pv->diffs.back().blob.a.id = line.mid(6, 40);
							}
							pv->diffs.back().blob.b.id = line.mid(48, 40);
						}
						break;
					case HeaderCode::a:
						if (itemtype != ItemType::NewFile) {
							pv->diffs.back().blob.a.path = GetPath(line);
						}
						break;
					case HeaderCode::b:
						pv->diffs.back().blob.b.path = GetPath(line);
						break;
					}
				}
			}
		}
	}
}

void MainWindow::makeDiff2(GitPtr g, QString const &id, QList<Git::Diff> *out) // バイナリファイルに対応するため、diff処理を別クラスにした
{
	Q_ASSERT(g);
	GitDiff dm;
	dm.diff(g, id, out);
}

void MainWindow::updateFilesList(QString const &old_id, QString const &new_id, bool singlelist)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	clearFileList();
	if (!singlelist) showFileList(false);

#if 0 // そのうち消す
	makeDiff(old_id, new_id);
#else
	makeDiff2(g, new_id, &pv->diffs);
#endif

	if (old_id.isEmpty() || new_id.isEmpty()) {
		std::map<QString, int> diffmap;

		for (int idiff = 0; idiff < pv->diffs.size(); idiff++) {
			Git::Diff const &diff = pv->diffs[idiff];
			QString filename;
			if (diff.diff.startsWith("diff")) {
				ushort const *left = diff.diff.utf16();
				ushort const *right = left + diff.diff.size();
				left += 4;
				while (left + 1 < right) {
					if (left[0] == 'a' && left[1] == '/') break;
					left++;
				}
				ushort const *mid = left + (right - left) / 2;
				if (QChar(*mid).isSpace() && mid[1] == 'b' && mid[2] == '/') {
					left += 2;
					QString fn1 = QString::fromUtf16(left, mid - left);
					mid += 3;
					QString fn2 = QString::fromUtf16(mid, right - mid);
					if (fn1 == fn2) {
						filename = fn1;
					}
				}
			}
			if (!filename.isEmpty()) {
				diffmap[filename] = idiff;
			}
		}

		Git::FileStatusList stats = g->status();
		for (Git::FileStatus const &s : stats) {
			if (s.code() == Git::FileStatusCode::Unknown) {
				qDebug() << "something wrong...";
				continue;
			}
			auto AddItem = [&](QString const &prefix, int idiff){
				QString filename = s.path1();
				QListWidgetItem *item = new QListWidgetItem(prefix + filename);
				item->setData(FilePathRole, filename);
				item->setData(DiffIndexRole, idiff);
				item->setData(HunkIndexRole, -1);
				if (s.isStaged() && s.code_y() == ' ') {
					ui->listWidget_staged->addItem(item);
				} else {
					(singlelist ? ui->listWidget_files : ui->listWidget_unstaged)->addItem(item);
				}
			};
			int idiff = -1;
			QString header;
			if (s.code_x() == 'D' || s.code_y() == 'D') {
				header = "(del) ";
			} else {
				auto it = diffmap.find(s.path1());
				if (it != diffmap.end()) {
					header = "(chg) ";
					idiff = it->second;
				} else {
					header = "(??\?) "; // damn trigraph
				}
			}
			AddItem(header, idiff);
		}
	} else {
		for (int idiff = 0; idiff < pv->diffs.size(); idiff++) {
			Git::Diff const &diff = pv->diffs[idiff];

			QString header;
			if (!diff.blob.a.id.isEmpty()) {
				if (!diff.blob.b.id.isEmpty()) {
					header = "(chg) ";
				} else {
					header = "(del) ";
				}
			} else if (!diff.blob.b.id.isEmpty()) {
				header = "(add) ";
			} else {
				header = "(??\?) "; // damn trigraph
			}

			QString filename = diff.blob.a.path;
			if (filename.isEmpty()) {
				filename = pv->diffs[idiff].blob.b.path;
			}
			QListWidgetItem *item = new QListWidgetItem(header + filename);
			item->setData(FilePathRole, filename);
			item->setData(DiffIndexRole, idiff);
			item->setData(HunkIndexRole, -1);
			(singlelist ? ui->listWidget_files : ui->listWidget_unstaged)->addItem(item);
		}
	}
}

void MainWindow::prepareLogTableWidget()
{
	QStringList cols = {
		tr("Graph"),
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

	updateCommitGraph(); // コミットグラフを更新
}

QString MainWindow::currentRepositoryName() const
{
	return pv->current_repo.name;
}

Git::Branch MainWindow::currentBranch() const
{
	return pv->current_branch;
}

bool MainWindow::isValidWorkingCopy(GitPtr const &g) const
{
	return g && g->isValidWorkingCopy();
}



int MainWindow::limitLogCount() const
{
	return 10000;
}

QDateTime MainWindow::limitLogTime() const
{
	QDateTime t = QDateTime::currentDateTime();
	t = t.addYears(-3);
	return t;
}

void MainWindow::queryBranches(GitPtr g)
{
	Q_ASSERT((bool)g);
	pv->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.flags & Git::Branch::Current) {
			pv->current_branch = b;
		}
		pv->branch_map[b.id].append(b);
	}
}

void MainWindow::queryTags(GitPtr g)
{
	Q_ASSERT((bool)g);
	pv->tag_map.clear();
	QList<Git::Tag> tags = g->tags();
	for (Git::Tag const &t : tags) {
		pv->tag_map[t.id].push_back(t);
	}
}

void MainWindow::openRepository(bool waitcursor)
{
	if (waitcursor) {
		OverrideWaitCursor;
		openRepository(false);
		return;
	}

	clearLog();
	clearRepositoryInfo();

	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		updateHeadFilesList(true);

		// ログを取得
		pv->logs = g->log(limitLogCount(), limitLogTime());

		// ブランチとタグを取得
		queryBranches(g);
		queryTags(g);

		ui->label_repo_name->setText(currentRepositoryName());
		ui->label_branch_name->setText(currentBranch().name);
	} else {
		QString name = currentRepositoryName();
		ui->label_repo_name->setText(name.isEmpty() ? tr("Unknown") : name);
		ui->label_branch_name->setText("NOT FOUND");
	}

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(pv->current_branch.id);
		item.message = tr("Uncommited changes");
		pv->logs.insert(pv->logs.begin(), item);
	}

	prepareLogTableWidget();

	int index = 0;
	int count = pv->logs.size();

	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &pv->logs[index];
		{
			QTableWidgetItem *item = new QTableWidgetItem();
			item->setSizeHint(QSize(100, 20));
			item->setData(IndexRole, index);
			ui->tableWidget_log->setItem(row, 0, item);
		}
		int col = 1; // カラム0はコミットグラフなので、その次から
		auto AddColumn = [&](QString const &text, bool bold){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			if (bold) {
				QFont font = item->font();
				font.setBold(true);
				item->setFont(font);
			}
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};
		QString commit_id;
		QString datetime;
		QString author;
		QString message;
		bool bold = false;
		{
			if (Git::isUncommited(*commit)) {
				bold = true;
			} else {
				commit_id = commit->commit_id.mid(0, 7);
			}
			datetime = misc::makeDateTimeString(commit->commit_date);
			author = commit->author;
			message = commit->message;

			{ // branch
				QList<Git::Branch> list = findBranch(commit->commit_id);
				auto AbbrevName = [](QString const &name){
					QStringList sl = name.split('/');
					if (sl.size() == 1) return sl[0];
					QString newname;
					for (int i = 0; i < sl.size(); i++) {
						QString s = sl[i];
						if (i + 1 < sl.size()) {
							s = s.mid(0, 1);
						}
						if (i > 0) {
							newname += '/';
						}
						newname += s;
					}
					return newname;
				};
				for (Git::Branch const &b : list) {
					message += " {";
					message += AbbrevName(b.name);
					if (b.ahead > 0) {
						message += tr(", %1 ahead").arg(b.ahead);
					}
					if (b.behind > 0) {
						message += tr(", %1 behind").arg(b.behind);
					}
					message += '}';
				}
			}
			{ // tag
				QList<Git::Tag> list = findTag(commit->commit_id);
				for (Git::Tag const &t : list) {
					message += " {t:";
					message += t.name;
					message += '}';
				}
			}
		}
		if (false) { // メッセージ欄に、親コミットIDを表示する（デバッグ用）
			message.clear();
			if (true) {
				message += QString::number(row);
			} else {
				for (QString id : commit->parent_ids) {
					if (!message.isEmpty()) {
						message += ' ';
					}
					message += id;
				}
			}
		}
		AddColumn(commit_id, false);
		AddColumn(datetime, false);
		AddColumn(author, false);
		AddColumn(message, bold);
		ui->tableWidget_log->setRowHeight(row, 24);
		index++;
	}
	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	ui->tableWidget_log->setCurrentCell(0, 0);

	udpateButton();
}

void MainWindow::udpateButton()
{
	Git::Branch b = currentBranch();

	int n;

	n = -1;
	if (b.ahead > 0 && !isThereUncommitedChanges()) n = b.ahead;
	ui->toolButton_push->setNumber(n);

	n = -1;
	if (b.behind > 0 && !isThereUncommitedChanges()) n = b.behind;
	ui->toolButton_pull->setNumber(n);
}

void MainWindow::autoOpenRepository(QString dir)
{
	dir = QDir(dir).absolutePath();

	auto Open = [&](RepositoryItem const &item){
		pv->current_repo = item;
		openRepository();
	};

	for (RepositoryItem const &item : pv->repos) {
		Qt::CaseSensitivity cs = Qt::CaseSensitive;
#ifdef Q_OS_WIN
		cs = Qt::CaseInsensitive;
#endif
		if (dir.compare(item.local_dir, cs) == 0) {
			Open(item);
			return;
		}
	}

	RepositoryItem newitem;
	GitPtr g = git(dir);
	if (isValidWorkingCopy(g)) {
		ushort const *left = dir.utf16();
		ushort const *right = left + dir.size();
		if (right[-1] == '/' || right[-1] == '\\') {
			right--;
		}
		ushort const *p = right;
		while (left + 1 < p && !(p[-1] == '/' || p[-1] == '\\')) p--;
		if (p < right) {
			newitem.local_dir = dir;
			newitem.name = QString::fromUtf16(p, right - p);
			saveRepositoryBookmark(newitem);
			Open(newitem);
			return;
		}
	}
}

void MainWindow::openSelectedRepository()
{
	QTreeWidgetItem *ite = ui->treeWidget_repos->currentItem();
	RepositoryItem const *item = repositoryItem(ite);
	if (item) {
		pv->current_repo = *item;
		openRepository();
	}
}

void MainWindow::resizeEvent(QResizeEvent *)
{
	updateVerticalScrollBar();
}

QString MainWindow::getBookmarksFilePath() const
{
	return application_data_dir / "bookmarks.xml";
}

void MainWindow::on_action_add_all_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->git("add --all");
	updateHeadFilesList(false);
}

void MainWindow::commit(bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	while (1) {
		TextEditDialog dlg;
		dlg.setWindowTitle(tr("Commit"));
		if (amend) {
			dlg.setText(pv->logs[0].message);
		}
		if (dlg.exec() == QDialog::Accepted) {
			QString text = dlg.text();
			if (text.isEmpty()) {
				QMessageBox::warning(this, tr("Commit"), tr("Commit message can not be omitted."));
				continue;
			}
			if (amend) {
				g->commit_amend_m(text);
			} else {
				g->commit(text);
			}
			openRepository();
			break;
		} else {
			break;
		}
	}
}

void MainWindow::commit_amend()
{
	commit(true);
}


void MainWindow::on_action_commit_triggered()
{
	commit();
}

void MainWindow::on_action_fetch_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;

	g->fetch();
	openRepository(false);
}

void MainWindow::on_action_push_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;

	g->push();
	openRepository(false);
}

void MainWindow::on_action_pull_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;

	g->pull();
	openRepository(false);
}

void MainWindow::on_toolButton_push_clicked()
{
	ui->action_push->trigger();
}

void MainWindow::on_toolButton_pull_clicked()
{
	ui->action_pull->trigger();
}

void MainWindow::on_action_set_remote_origin_url_triggered()
{
	QString newurl;
	Git git(pv->gcx, currentWorkingCopyDir());
	git.git("remote set-url origin " + newurl);
}

void MainWindow::on_action_config_global_credential_helper_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->git("config --global credential.helper");
	QString text = g->resultText().trimmed();
	ConfigCredentialHelperDialog dlg(this);
	dlg.setHelper(text);
	if (dlg.exec() == QDialog::Accepted) {
		text = dlg.helper();
		bool ok = true;
		if (text.isEmpty()) {
			text = "\"\"";
		} else {
			auto isValidChar = [](ushort c){
				if (QChar::isLetterOrNumber(c)) return true;
				if (c == '_') return true;
				return false;
			};
			ushort const *begin = text.utf16();
			ushort const *end = begin + text.size();
			ushort const *ptr = begin;
			while (ptr < end) {
				if (!isValidChar(*ptr)) {
					ok = false;
					break;
				}
				ptr++;
			}
		}
		if (!ok) {
			QMessageBox::warning(this, tr("Config credential helper"), tr("Invalid credential helper name"));
			return;
		}
		g->git("config --global credential.helper " + text);
	}
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
	openSelectedRepository();
}

void MainWindow::onDiffWidgetWheelScroll(int lines)
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

bool MainWindow::askAreYouSureYouWantToRun(QString const &title, QString const &command)
{
	QString message = tr("Are you sure you want to run the following command ?");
	QString text = "%1\n\n> %2";
	text = text.arg(message).arg(command);
	return QMessageBox::warning(this, title, text, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

void MainWindow::revertFile(QStringList const &paths)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (paths.isEmpty()) {
		// nop
	} else {
		QString cmd = "git checkout -- \"" + paths[0] + '\"';
		if (askAreYouSureYouWantToRun(tr("Revert a file"), cmd)) {
			for (QString const &path : paths) {
				g->revertFile(path);
			}
			openRepository();
		}
	}
}

void MainWindow::revertAllFiles()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString cmd = "git reset --hard HEAD";
	if (askAreYouSureYouWantToRun(tr("Revert all files"), cmd)) {
		g->revertAllFiles();
		openRepository();
	}
}

QStringList MainWindow::selectedStagedFiles() const
{
	QStringList list;
	QList<QListWidgetItem *> items = ui->listWidget_staged->selectedItems();
	for (QListWidgetItem *item : items) {
		QString path = item->data(FilePathRole).toString();
		list.push_back(path);
	}
	return list;
}

QStringList MainWindow::selectedUnstagedFiles() const
{
	QStringList list;
	QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
	for (QListWidgetItem *item : items) {
		QString path = item->data(FilePathRole).toString();
		list.push_back(path);
	}
	return list;
}

void MainWindow::for_each_selected_staged_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedStagedFiles()) {
		fn(path);
	}
}

void MainWindow::for_each_selected_unstaged_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedUnstagedFiles()) {
		fn(path);
	}
}

void MainWindow::on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
	if (!items.isEmpty()) {
		QMenu menu;
		QAction *a_stage = menu.addAction("Stage");
		QAction *a_revert = menu.addAction("Revert");
		QAction *a_ignore = menu.addAction("Ignore");
		QAction *a_remove = menu.addAction("Remove");
		QPoint pt = ui->listWidget_unstaged->mapToGlobal(pos);
		QAction *a = menu.exec(pt);
		if (a) {
			if (a == a_stage) {
				for_each_selected_unstaged_files([&](QString const &path){
					g->stage(path);
				});
				updateHeadFilesList(false);
			} else if (a == a_revert) {
				QStringList paths;
				for_each_selected_unstaged_files([&](QString const &path){
					paths.push_back(path);
				});
				revertFile(paths);
			} else if (a == a_ignore) {
				QString text;
				for_each_selected_unstaged_files([&](QString const &path){
					if (path == ".gitignore") {
						// skip
					} else {
						text += path + '\n';
					}
				});
				QString gitignore_path = currentWorkingCopyDir() / ".gitignore";
				if (TextEditDialog::editFile(this, gitignore_path, ".gitignore")) {
					updateHeadFilesList(false);
				}
			} else if (a == a_remove) {
				for_each_selected_unstaged_files([&](QString const &path){
					g->removeFile(path);
					g->chdirexec([&](){
						QFile(path).remove();
						return true;
					});
				});
				updateHeadFilesList(false);
			}
		}
	}
}

void MainWindow::on_listWidget_staged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = ui->listWidget_staged->currentItem();
	if (item) {
		QString path = item->data(FilePathRole).toString();
		QString fullpath = currentWorkingCopyDir() / path;
		if (QFileInfo(fullpath).isFile()) {
			QMenu menu;
			QAction *a_unstage = menu.addAction("Unstage");
			QPoint pt = ui->listWidget_staged->mapToGlobal(pos);
			QAction *a = menu.exec(pt);
			if (a) {
				if (a == a_unstage) {
					g->unstage(path);
					updateHeadFilesList(false);
				}
			}
		}
	}
}

void MainWindow::on_action_open_existing_working_copy_triggered()
{
	QString dir = defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Add existing working copy"), dir);

	RepositoryItem item;
	item.local_dir = dir;
	item.name = makeRepositoryName(dir);
	saveRepositoryBookmark(item);
}

void MainWindow::on_action_view_refresh_triggered()
{
	openRepository();
}

void MainWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	pv->update_files_list_counter = 200;
	ui->widget_diff_pixmap->clear(false);
	ui->listWidget_unstaged->clear();
	ui->listWidget_staged->clear();
	ui->listWidget_files->clear();
}

void MainWindow::on_treeWidget_repos_customContextMenuRequested(const QPoint &pos)
{
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	RepositoryItem const *repo = repositoryItem(treeitem);
	int index = treeitem->data(0, IndexRole).toInt();
	if (isGroupItem(treeitem)) {
		QMenu menu;
		QAction *a_add_new_group = menu.addAction(tr("&Add new group"));
		QAction *a_delete_group = menu.addAction(tr("&Delete group"));
		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_add_new_group) {
				QTreeWidgetItem *child = newQTreeWidgetFolderItem(tr("New folder"));
				treeitem->addChild(child);
				child->setFlags(child->flags() | Qt::ItemIsEditable);
				ui->treeWidget_repos->setCurrentItem(child);
				return;
			}
			if (a == a_delete_group) {
				QTreeWidgetItem *parent = treeitem->parent();
				if (parent) {
					int i = parent->indexOfChild(treeitem);
					delete parent->takeChild(i);
				} else {
					int i = ui->treeWidget_repos->indexOfTopLevelItem(treeitem);
					delete ui->treeWidget_repos->takeTopLevelItem(i);
				}
				refrectRepositories();
				return;
			}
		}
	} else if (repo) {
		QString open_terminal = tr("Open &terminal");
		QString open_commandprompt = tr("Open command promp&t");
		QMenu menu;
		QAction *a_open_folder = menu.addAction(tr("Open &folder"));
#ifdef Q_OS_WIN
		QAction *a_open_terminal = menu.addAction(open_commandprompt);
#else
		QAction *a_open_terminal = menu.addAction(open_terminal);
#endif
		QAction *a_remove = menu.addAction(tr("&Remove"));
		QAction *a_property = menu.addAction(tr("&Property"));
		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_open_folder) {
				QDesktopServices::openUrl(repo->local_dir);
				return;
			}
			if (a == a_open_terminal) {
				Terminal::open(repo->local_dir);
				return;
			}
			if (a == a_remove) {
				pv->repos.erase(pv->repos.begin() + index);
				saveRepositoryBookmarks();
				updateRepositoriesList();
				return;
			}
			if (a == a_property) {
				RepositoryPropertyDialog dlg(this, *repo);
				dlg.exec();
				return;
			}
		}
	}
}

void MainWindow::on_toolButton_stage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->stage(selectedUnstagedFiles());
	updateHeadFilesList(false);
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->unstage(selectedStagedFiles());
	updateHeadFilesList(false);
}

void MainWindow::on_toolButton_select_all_clicked()
{
	if (ui->listWidget_unstaged->count() > 0) {
		ui->listWidget_unstaged->setFocus();
		ui->listWidget_unstaged->selectAll();
	} else if (ui->listWidget_staged->count() > 0) {
		ui->listWidget_staged->setFocus();
		ui->listWidget_staged->selectAll();
	}
}

void MainWindow::on_toolButton_commit_clicked()
{
	ui->action_commit->trigger();
}

bool MainWindow::editFile(const QString &path, QString const &title)
{
	return TextEditDialog::editFile(this, path, title);
}

struct Task {
	int index = 0;
	int parent = 0;
	Task()
	{
	}
	Task(int index, int parent)
		: index(index)
		, parent(parent)
	{
	}
};

struct Element {
	int depth = 0;
	std::vector<int> indexes;
};

void MainWindow::updateCommitGraph()
{
	const size_t LogCount = pv->logs.size();
	// 樹形図情報を構築する
	if (LogCount > 0) {
		auto LogItem = [&](size_t i)->Git::CommitItem &{ return pv->logs[i]; };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : pv->logs) {
			item.marker_depth = UNKNOWN;
		}
		// コミットハッシュを検索して、親コミットのインデックスを求める
		for (size_t i = 0; i < LogCount; i++) {
			Git::CommitItem *item = &LogItem(i);
			item->parent_lines.clear();
			if (item->parent_ids.empty()) {
				item->resolved = true;
			} else {
				for (int j = 0; j < item->parent_ids.size(); j++) { // 親の数だけループ
					QString const &parent_id = item->parent_ids[j]; // 親のハッシュ値
					for (size_t k = i + 1; k < LogCount; k++) { // 親を探す
						if (LogItem(k).commit_id == parent_id) { // ハッシュ値が一致したらそれが親
							item->parent_lines.push_back(k); // インデックス値を記憶
							LogItem(k).has_child = true;
							LogItem(k).marker_depth = KNOWN;
							item->resolved = true;
							break;
						}
					}
				}
			}
		}
		std::vector<Element> elements; // 線分リスト
		{ // 線分リストを作成する
			std::deque<Task> tasks; // 未処理タスクリスト
			{
				for (size_t i = 0; i < LogCount; i++) {
					Git::CommitItem *item = &LogItem(i);
					if (item->marker_depth == UNKNOWN) {
						int n = item->parent_lines.size(); // 最初のコミットアイテム
						for (int j = 0; j < n; j++) {
							tasks.push_back(Task(i, j)); // タスクを追加
						}
					}
					item->marker_depth = UNKNOWN;
				}
			}
			while (!tasks.empty()) { // タスクが残っているならループ
				Element e;
				Task task;
				{ // 最初のタスクを取り出す
					task = tasks.front();
					tasks.pop_front();
				}
				e.indexes.push_back(task.index); // 先頭のインデックスを追加
				int index = LogItem(task.index).parent_lines[task.parent].index; // 開始インデックス
				while (index > 0 && (size_t)index < LogCount) { // 最後に到達するまでループ
					e.indexes.push_back(index); // インデックスを追加
					int n = LogItem(index).parent_lines.size(); // 親の数
					if (n == 0) break; // 親がないなら終了
					Git::CommitItem *item = &LogItem(index);
					if (item->marker_depth == KNOWN) break; // 既知のアイテムに到達したら終了
					item->marker_depth = KNOWN; // 既知のアイテムにする
					for (int i = 1; i < n; i++) {
						tasks.push_back(Task(index, i)); // タスク追加
					}
					index = LogItem(index).parent_lines[0].index; // 次の親（親リストの先頭の要素）
				}
				if (e.indexes.size() >= 2) {
					elements.push_back(e);
				}
			}
		}
		// 線情報をクリア
		for (Git::CommitItem &item : pv->logs) {
			item.marker_depth = -1;
			item.parent_lines.clear();
		}
		// マークと線の深さを決める
		if (!elements.empty()) {
			{ // 優先順位を調整する
				std::sort(elements.begin(), elements.end(), [](Element const &left, Element const &right){
					int i = 0;
					{ // 長いものを優先して左へ
						int l = left.indexes.back() - left.indexes.front();
						int r = right.indexes.back() - right.indexes.front();
						i = r - l; // 降順
					}
					if (i == 0) {
						// コミットが新しいものを優先して左へ
						int l = left.indexes.front();
						int r = right.indexes.front();
						i = l - r; // 昇順
					}
					return i < 0;
				});
				// 子の無いブランチ（タグ等）が複数連続しているとき、古いコミットを右に寄せる
				{
					for (size_t i = 0; i + 1 < elements.size(); i++) {
						Element *e = &elements[i];
						int index1 = e->indexes.front();
						if (index1 > 0 && !LogItem(index1).has_child) { // 子がない
							// 新しいコミットを探す
							for (size_t j = i + 1; j < elements.size(); j++) { // 現在位置より後ろを探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 == index2 + 1) { // 一つだけ新しいコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置に挿入
								}
							}
							// 古いコミットを探す
							size_t j = 0;
							while (j < i) { // 現在位置より前を探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 + 1 == index2) { // 一つだけ古いコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置の次に挿入
									index1 = index2;
									f = e;
								} else {
									j++;
								}
							}
						}
					}
				}
			}
			{ // 最初の線は深さを0にする
				Element *e = &elements.front();
				for (size_t i = 0; i < e->indexes.size(); i++) {
					int index = e->indexes[i];
					LogItem(index).marker_depth = 0; // マークの深さを設定
					e->depth = 0; // 線の深さを設定
				}
			}
			// 最初以外の線分の深さを決める
			for (size_t i = 1; i < elements.size(); i++) { // 最初以外をループ
				Element *e = &elements[i];
				int depth = 1;
				while (1) { // 失敗したら繰り返し
					for (size_t j = 0; j < i; j++) { // 既に処理済みの線を調べる
						Element const *f = &elements[j]; // 検査対象
						if (e->indexes.size() == 2) { // 二つしかない場合
							int from = e->indexes[0]; // 始点
							int to = e->indexes[1];   // 終点
							if (LogItem(from).has_child) {
								for (size_t k = 0; k + 1 < f->indexes.size(); k++) { // 検査対象の全ての線分を調べる
									int curr = f->indexes[k];
									int next = f->indexes[k + 1];
									if (from > curr && to == next) { // 決定済みの線に直結できるか判定
										e->indexes.back() = from + 1; // 現在の一行下に直結する
										e->depth = elements[j].depth; // 接続先の深さ
										goto DONE; // 決定
									}
								}
							}
						}
						if (depth == f->depth) { // 同じ深さ
							if (e->indexes.back() > f->indexes.front() && e->indexes.front() < f->indexes.back()) { // 重なっている
								goto FAIL; // この深さには線を置けないのでやりなおし
							}
						}
					}
					for (size_t j = 0; j < e->indexes.size(); j++) {
						int index = e->indexes[j];
						Git::CommitItem *item = &LogItem(index);
						if (j == 0 && item->has_child) { // 最初のポイントで子がある場合
							// nop
						} else if ((j > 0 && j + 1 < e->indexes.size()) || item->marker_depth < 0) { // 最初と最後以外、または、未確定の場合
							item->marker_depth = depth; // マークの深さを設定
						}
					}
					e->depth = depth; // 深さを決定
					goto DONE; // 決定
FAIL:;
					depth++; // 一段深くして再挑戦
				}
DONE:;
			}
			// 線情報を生成する
			for (size_t i = 0; i < elements.size(); i++) {
				Element const &e = elements[i];
				auto ColorNumber = [&](){ return e.depth; };
				size_t count = e.indexes.size();
				for (size_t j = 0; j + 1 < count; j++) {
					int curr = e.indexes[j];
					int next = e.indexes[j + 1];
					TreeLine line(next, e.depth);
					line.color_number = ColorNumber();
					line.bend_early = (j + 2 < count || !LogItem(next).resolved);
					if (j + 2 == count) {
						if (count > 2 || !LogItem(curr).has_child) { // 直結ではない、または、子がない
							line.depth = LogItem(next).marker_depth; // 合流する先のアイテムの深さと同じにする
						}
					}
					LogItem(curr).parent_lines.push_back(line); // 線を追加
				}
			}
		}
	}
}

QList<Git::Branch> MainWindow::findBranch(QString const &id)
{
	auto it = pv->branch_map.find(id);
	if (it != pv->branch_map.end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QList<Git::Tag> MainWindow::findTag(QString const &id)
{
	auto it = pv->tag_map.find(id);
	if (it != pv->tag_map.end()) {
		return it->second;
	}
	return QList<Git::Tag>();
}

void MainWindow::on_action_edit_global_gitconfig_triggered()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
	QString path = dir / ".gitconfig";
	editFile(path, ".gitconfig");
}

void MainWindow::on_action_edit_git_config_triggered()
{
	QString dir = currentWorkingCopyDir();
	QString path = dir / ".git/config";
	editFile(path, ".git/config");
}

void MainWindow::on_action_edit_gitignore_triggered()
{
	QString dir = currentWorkingCopyDir();
	QString path = dir / ".gitignore";
	editFile(path, ".gitignore");
}

int MainWindow::selectedLogIndex() const
{
	int i = ui->tableWidget_log->currentRow();
	if (i >= 0 && i < (int)pv->logs.size()) {
		return i;
	}
	return -1;
}

Git::CommitItem const *MainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	if (i >= 0 && i < (int)pv->logs.size()) {
		return &pv->logs[i];
	}
	return nullptr;
}

void MainWindow::doUpdateFilesList()
{
	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;
	int row = item->data(IndexRole).toInt();
	int logs = (int)pv->logs.size();
	if (row < logs) {
		if (Git::isUncommited(pv->logs[row])) {
			updateHeadFilesList(false);
		} else {
			QString newer = pv->logs[row].commit_id;
			QString older;
			if (pv->logs[row].parent_ids.size() > 0) {
				older = pv->logs[row].parent_ids[0];
			}
			updateFilesList(older, newer, true);
		}
	}
}

void MainWindow::changeLog(QListWidgetItem *item, bool uncommited)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	clearDiff();
	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < pv->diffs.size()) {
		Git::Diff const &diff = pv->diffs[idiff];
		QByteArray ba;
		if (diff.blob.a.id.isEmpty()) {
			g->cat_file(diff.blob.b.id, &ba);
			setDataAsNewFile(ba);
		} else {
			g->cat_file(diff.blob.a.id, &ba);
			setTextDiffData(ba, diff, uncommited, currentWorkingCopyDir());
		}
		ui->verticalScrollBar->setValue(0);
		updateVerticalScrollBar();
		ui->widget_diff_pixmap->clear(false);
		updateSliderCursor();
		ui->widget_diff_pixmap->update();
		updateSliderCursor();
	}
}

void MainWindow::on_listWidget_unstaged_currentRowChanged(int currentRow)
{
	bool uncommited = isThereUncommitedChanges() && currentRow == 0;
	QListWidgetItem *item = ui->listWidget_unstaged->item(currentRow);
	changeLog(item, uncommited);
}

void MainWindow::on_listWidget_files_currentRowChanged(int currentRow)
{
	bool uncommited = isThereUncommitedChanges() && currentRow == 0;
	QListWidgetItem *item = ui->listWidget_files->item(currentRow);
	changeLog(item, uncommited);
}

void MainWindow::setGitCommand(QString const &path, bool save)
{
	if (save) {
		MySettings s;
		s.beginGroup("Global");
		s.setValue("GitCommand", path);
		s.endGroup();
	}
	pv->gcx.git_command = path;
}

bool MainWindow::selectGitCommand()
{
#ifdef Q_OS_WIN
	char const *exe = "git.exe";
#else
	char const *exe = "git";
#endif


	QStringList list;
	{
		std::vector<std::string> vec;
		FileUtil::which(exe, &vec);

		std::sort(vec.begin(), vec.end());
		auto it = std::unique(vec.begin(), vec.end());
		vec = std::vector<std::string>(vec.begin(), it);

		for (std::string const &s : vec) {
			list.push_back(QString::fromStdString(s));
		}
	}

	QString path = pv->gcx.git_command;
	SelectGitCommandDialog dlg(this, path, list);
	if (dlg.exec() == QDialog::Accepted) {
		path = dlg.selectedFile();
		QFileInfo info(path);
		if (info.isExecutable()) {
			setGitCommand(path, true);
			pv->gcx.git_command = path;
		}
		return true;
	}
	return false;
}

void MainWindow::checkGitCommand()
{
	while (1) {
		QFileInfo info(pv->gcx.git_command);
		if (info.isExecutable()) {
			break; // ok
		}
		if (!selectGitCommand()) {
			close();
			break;
		}
	}
}

void MainWindow::timerEvent(QTimerEvent *)
{
	pv->interval_250ms_counter += pv->timer_interval_ms;
	if (pv->interval_250ms_counter >= 250) {
		pv->interval_250ms_counter -= 250;

		checkGitCommand();
	}

	if (pv->update_files_list_counter > 0) {
		if (pv->update_files_list_counter > pv->timer_interval_ms) {
			pv->update_files_list_counter -= pv->timer_interval_ms;
		} else {
			pv->update_files_list_counter = 0;
			doUpdateFilesList();
		}
	}
}

bool MainWindow::event(QEvent *event)
{
	return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->treeWidget_repos) {
		if (event->type() == QEvent::KeyPress) {
			QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
			Q_ASSERT(e);
			int k = e->key();
			if (k == Qt::Key_Enter || k == Qt::Key_Return) {
				openSelectedRepository();
				return true;
			}
		}
	}
	return false;
}

void MainWindow::onDiffWidgetResized()
{
	updateSliderCursor();
	updateVerticalScrollBar();
}

bool MainWindow::saveFileAs(QString const &srcpath, QString const &dstpath)
{
	QFile f(srcpath);
	if (f.open(QFile::ReadOnly)) {
		QByteArray ba = f.readAll();
		QFile file(dstpath);
		if (file.open(QFile::WriteOnly)) {
			file.write(ba);
			file.close();
			return true;
		}
	}
	return false;
}

bool MainWindow::saveBlobAs(QString const &id, QString const &dstpath)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	QByteArray ba;
	if (g->cat_file(id, &ba)) {
		QFile file(dstpath);
		if (file.open(QFile::WriteOnly)) {
			file.write(ba);
			file.close();
			return true;
		}
	}
	return false;
}

void MainWindow::on_action_edit_settings_triggered()
{
	SettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		pv->appsettings = dlg.settings();
		setGitCommand(pv->appsettings.git_command, false);
	}
}

void MainWindow::on_action_branch_new_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NewBranchDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		g->createBranch(name);
	}
}

void MainWindow::on_action_branch_checkout_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<Git::Branch> branches = g->branches();
	CheckoutBranchDialog dlg(this, branches);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		if (!name.isEmpty()) {
			g->checkoutBranch(name);
			openRepository();
		}
	}
}

void MainWindow::on_action_branch_merge_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<Git::Branch> branches = g->branches();
	MergeBranchDialog dlg(this, branches);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		if (!name.isEmpty()) {
			g->mergeBranch(name);
		}
	}

}

void MainWindow::on_action_clone_triggered()
{
	GitPtr g = std::shared_ptr<Git>(new Git(pv->gcx, QString()));
	CloneDialog dlg(this, g, defaultWorkingDir());
	if (dlg.exec() == QDialog::Accepted) {
		QString dir = dlg.workingDir();
		RepositoryItem item;
		item.local_dir = dir;
		item.name = makeRepositoryName(dir);
		saveRepositoryBookmark(item);
		pv->current_repo = item;
		openRepository();
	}
}

void MainWindow::on_action_about_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();

}

void MainWindow::on_toolButton_clone_clicked()
{
	ui->action_clone->trigger();
}

void MainWindow::on_toolButton_fetch_clicked()
{
	ui->action_fetch->trigger();
}

void MainWindow::on_comboBox_filter_currentTextChanged(const QString &text)
{
	pv->repository_filter_text = text;
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	ui->comboBox_filter->clearEditText();
	ui->treeWidget_repos->setFocus();
}

void MainWindow::deleteTags(QStringList const &tagnames)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!tagnames.isEmpty()) {
		OverrideWaitCursor;
		for (QString const &name : tagnames) {
			g->delete_tag(name, true);
		}
		openRepository(false);
	}
}

void MainWindow::deleteTags(const Git::CommitItem &commit)
{
	auto it = pv->tag_map.find(commit.commit_id);
	if (it != pv->tag_map.end()) {
		QStringList names;
		QList<Git::Tag> const &tags = it->second;
		for (Git::Tag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(names);
	}
}

void MainWindow::deleteSelectedTags()
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		QList<Git::Tag> list = findTag(commit->commit_id);
		if (!list.isEmpty()) {
			DeleteTagsDialog dlg(this, list);
			if (dlg.exec() == QDialog::Accepted) {
				QStringList list = dlg.selectedTags();
				deleteTags(list);
			}
		}
	}
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		int row = selectedLogIndex();
		QMenu menu;
		QAction *a_property = menu.addAction(tr("&Property"));
		QAction *a_edit_comment = nullptr;
		if (row == 0 && currentBranch().ahead > 0) {
			a_edit_comment = menu.addAction(tr("Edit comment..."));
		}
		QAction *a_delete_tags = nullptr;
		if (pv->tag_map.find(commit->commit_id) != pv->tag_map.end()) {
			a_delete_tags = menu.addAction(tr("Delete tags"));
		}
		QAction *a = menu.exec(ui->tableWidget_log->viewport()->mapToGlobal(pos) + QPoint(8, -8));
		if (a) {
			if (a == a_property) {
				CommitPropertyDialog dlg(this, *commit);
				dlg.exec();
				return;
			}
			if (a == a_edit_comment) {
				commit_amend();
				return;
			}
			if (a == a_delete_tags) {
				deleteSelectedTags();
				return;
			}
		}
	}
}

void MainWindow::on_action_tag_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	EditTagDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		OverrideWaitCursor;
		g->tag(dlg.text());
		if (dlg.isPushChecked()) {
			g->push(true);
		}
		openRepository(false);
	}
}

void MainWindow::on_action_tag_push_all_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;
	g->push(true);
}


void MainWindow::on_action_tag_delete_triggered()
{
	deleteSelectedTags();
}

// diff

DiffWidgetData *MainWindow::getDiffWidgetData()
{
	return &pv->diff_widget_data;
}

DiffWidgetData::DiffData *MainWindow::diffdata()
{
	return &pv->diff_widget_data.diffdata;
}

DiffWidgetData::DiffData const *MainWindow::diffdata() const
{
	return &pv->diff_widget_data.diffdata;
}

DiffWidgetData::DrawData *MainWindow::drawdata()
{
	return &pv->diff_widget_data.drawdata;
}

DiffWidgetData::DrawData const *MainWindow::drawdata() const
{
	return &pv->diff_widget_data.drawdata;
}


int MainWindow::totalTextLines() const
{
	return diffdata()->left_lines.size();
}

int MainWindow::fileviewScrollPos() const
{
	return drawdata()->scrollpos;
}

int MainWindow::fileviewHeight() const
{
	return ui->widget_diff_left->height();
}

int MainWindow::visibleLines() const
{
	int n = 0;
	if (drawdata()->line_height > 0) {
		n = fileviewHeight() / drawdata()->line_height;
		if (n < 1) n = 1;
	}
	return n;
}

void MainWindow::updateVerticalScrollBar()
{
	QScrollBar *sb = ui->verticalScrollBar;
	if (drawdata()->line_height > 0) {
		int lines_per_widget = fileviewHeight() / drawdata()->line_height;
		if (lines_per_widget < diffdata()->left_lines.size() + 1) {
			sb->setRange(0, diffdata()->left_lines.size() - lines_per_widget + 1);
			sb->setPageStep(lines_per_widget);
			return;
		}
	}
	sb->setRange(0, 0);
	sb->setPageStep(0);
}

QString MainWindow::formatLine(QString const &text, bool diffmode)
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

void MainWindow::setDiffText_(QList<TextDiffLine> const &left, QList<TextDiffLine> const &right, bool diffmode)
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


void MainWindow::clearDiff()
{
	*diffdata() = DiffWidgetData::DiffData();
	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

void MainWindow::setDataAsNewFile(QByteArray const &ba)
{
	clearDiff();

	if (ba.isEmpty()) {
		diffdata()->original_lines.clear();
	} else {
		diffdata()->original_lines = misc::splitLines(ba, [](char const *ptr, size_t len){ return QString::fromUtf8(ptr, len); });
	}

	QList<TextDiffLine> left_newlines;
	QList<TextDiffLine> right_newlines;

	for (QString const &line : diffdata()->original_lines) {
		QString text = '+' + line;
		left_newlines.push_back(QString());
		right_newlines.push_back(text);
	}

	setDiffText_(left_newlines, right_newlines, true);
}

void MainWindow::setTextDiffData(QByteArray const &ba, Git::Diff const &diff, bool uncommited, QString const &workingdir)
{
	clearDiff();
	diffdata()->left = diff.blob.a;
	diffdata()->right = diff.blob.b;
	if (uncommited) {
		QString path = workingdir / diffdata()->right.path;
		diffdata()->right.id = QString(PATH_PREFIX) + path;
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


void MainWindow::scrollTo(int value)
{
	drawdata()->scrollpos = value;
	ui->widget_diff_left->update(ViewType::Left);
	ui->widget_diff_right->update(ViewType::Right);
}

QPixmap MainWindow::makeDiffPixmap(ViewType side, int width, int height)
{
	auto MakePixmap = [&](QList<TextDiffLine> const &lines, int w, int h){
		const int scale = 1;
		QPixmap pixmap = QPixmap(w, h * scale);
		pixmap.fill(Qt::white);
		QPainter pr(&pixmap);
		auto Loop = [&](std::function<QColor(TextDiffLine::Type)> getcolor){
			int i = 0;
			while (i < lines.size()) {
				TextDiffLine::Type type = lines[i].type;
				int j = i + 1;
				if (type != TextDiffLine::Unchanged) {
					while (j < lines.size()) {
						if (lines[j].type != type) break;
						j++;
					}
					int y = i * pixmap.height() / lines.size();
					int z = j * pixmap.height() / lines.size();
					if (z == y) z = y + 1;
					QColor color = getcolor(type);
					if (color.isValid()) pr.fillRect(0, y, w, z - y, color);
				}
				i = j;
			}
		};
		DiffWidgetData::DrawData const *dd = drawdata();
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Unknown: return dd->bgcolor_gray;
			}
			return QColor();
		});
		Loop([&](TextDiffLine::Type t)->QColor{
			switch (t) {
			case TextDiffLine::Add: return dd->bgcolor_add_dark;
			case TextDiffLine::Del: return dd->bgcolor_del_dark;
			}
			return QColor();
		});
		if (scale == 1) return pixmap;
		return pixmap.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	};
	if (side == ViewType::Left)  return MakePixmap(diffdata()->left_lines, width, height);
	if (side == ViewType::Right) return MakePixmap(diffdata()->right_lines, width, height);
	return QPixmap();
}

//

QString MainWindow::tempfileHeader() const
{
	QString name = "jp_soramimi_Guitar_%1_";
	return name.arg(QApplication::applicationPid());

}

void MainWindow::deleteTempFiles()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString name = tempfileHeader();
	QDirIterator it(dir, { name + "*" });
	while (it.hasNext()) {
		QString path = it.next();
		QFile::remove(path);
		qDebug() << path;
	}
}

QString MainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(pv->temp_file_counter);
	pv->temp_file_counter++;
	return path;
}

void MainWindow::on_action_test_triggered()
{
	QString path = newTempFilePath();
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		file.close();
	}
//	deleteTempFiles();
}


