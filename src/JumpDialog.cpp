#include "JumpDialog.h"
#include "ui_JumpDialog.h"
#include "MyTableWidgetDelegate.h"
#include "ApplicationGlobal.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <MainWindow.h>
#include <QKeyEvent>
#include <QPainter>
#include <optional>

namespace {

class ItemDelegate : public MyTableWidgetDelegate {
private:
	QString filter_text_;
public:
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);
		opt.state &= ~QStyle::State_HasFocus; // セルのフォーカス枠は描画しない

		QString text;
		std::swap(text, opt.text);

		paint_bg(painter, opt, index);

		qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
		qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, opt.widget);

		struct Element {
			QString text;
			bool decorated = false;
		};
		std::vector<Element> elements;
		{
			if (filter_text_.isEmpty()) {
				Element e;
				e.text = text;
				elements.push_back(e);
			} else {
				int i = text.indexOf(filter_text_, 0, Qt::CaseInsensitive);
				if (i >= 0) {
					if (i > 0) {
						Element e;
						e.text = text.left(i);
						elements.push_back(e);
					}
					Element e;
					e.text = text.mid(i, filter_text_.size());
					e.decorated = true;
					elements.push_back(e);
					int j = i + filter_text_.size();
					if (j < text.size()) {
						Element e;
						e.text = text.mid(j);
						elements.push_back(e);
					}
				} else {
					Element e;
					e.text = text;
					elements.push_back(e);
				}
			}
		}

		int x = opt.rect.x() + 2;

		painter->setFont(opt.font);
		painter->setPen(opt.palette.color(QPalette::Text));

		for (Element const &e : elements) {
			QString text = e.text;
			int w = painter->fontMetrics().size(Qt::TextSingleLine, text).width();
			QRect rect(x, opt.rect.y(), w, opt.rect.height());
			if (e.decorated) {
				painter->fillRect(rect, global->appsettings.incremental_search_color.highlight_bg);
			}
			painter->drawText(rect, opt.displayAlignment, text); // テキストを描画
			x += w;
		}
	}

	void setFilterText(QString const &filter_text)
	{
		filter_text_ = filter_text;
	}
};

} // namespace

struct JumpDialog::Private {
	ItemDelegate delegate;
	QString filter_text;
	QString selected_name;

	NamedCommitList all_items;
	std::optional<NamedCommitList> filtered_items;

	std::unordered_map<std::string, CommitRecord const *> commit_record_map;

	QStringList header;
};

JumpDialog::JumpDialog(QWidget *parent, const NamedCommitList &items, CommitRecords::Vector const *commit_records)
	: QDialog(parent)
	, ui(new Ui::JumpDialog)
	, m(new Private)
{
	ui->setupUi(this);

	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	qApp->installEventFilter(this);
	installEventFilter(this);

	// make commit record map
	for (CommitRecord const &r : *commit_records) {
		m->commit_record_map[r.commit_id()] = &r;
	}

	ui->tableWidget->setItemDelegate(&m->delegate);

	for (NamedCommitItem const &item : items) {
		NamedCommitItem newitem = item;
		if (newitem.type == NamedCommitItem::Type::BranchRemote) {
			if (!newitem.remote.empty()) {
				newitem.name = "remotes" / newitem.remote / newitem.name;
			}
		} else if (newitem.type == NamedCommitItem::Type::Tag) {
			newitem.name = "tags" / newitem.name;
		}
		m->all_items.push_back(newitem);
	}
	sort(&m->all_items);

	m->header = QStringList{
		tr("Name"),
		tr("Date"),
		tr("Message"),
	};

	ui->tableWidget->setColumnCount(m->header.size());
	{
		for (int i = 0; i < m->header.size(); i++) {
			auto *p = new QTableWidgetItem();
			p->setText(m->header[i]);
			ui->tableWidget->setHorizontalHeaderItem(i, p);
		}
	}
	updateTable();

	ui->tableWidget->setFocus();
}

JumpDialog::~JumpDialog()
{
	delete m;
	delete ui;
}

MainWindow *JumpDialog::mainwindow()
{
	return global->mainwindow;
}

static constexpr int ASCII_BACKSPACE = 0x08;
static constexpr int ASCII_DELETE = 0xff;

bool JumpDialog::appendCharToFilterText(int k)
{
	if (k >= 0 && k < 128 && isalnum(k)) { // 英数字
		// thru
	} else if (k == Qt::Key_Backspace) {
		k = ASCII_BACKSPACE;
	} else if (k == Qt::Key_Delete) {
		k = ASCII_DELETE;
	} else {
		return false;
	}

	QString text = ui->lineEdit_filter->text();
	if (k == ASCII_BACKSPACE) {
		int i = text.size();
		if (i > 0) {
			text.remove(i - 1, 1);
		}
	} else if (k == ASCII_DELETE) {
		text.clear();
	} else if (QChar(k).isLetterOrNumber()) {
		text.append(QChar(k).toLower());
	}
	ui->lineEdit_filter->setText(text);
	m->delegate.setFilterText(text);

	ui->tableWidget->setCurrentCell(0, 0);
	return true;
}


bool JumpDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->tableWidget) {
		if (event->type() == QEvent::FocusIn) {
			int row = ui->tableWidget->currentRow();
			if (row < 0) {
				row = 0;
			}
			ui->tableWidget->setCurrentCell(row, 0);
			ui->tableWidget->selectRow(row);
			return true;
		}
		if (event->type() == QEvent::KeyPress) {
			QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
			Q_ASSERT(e);
			if (appendCharToFilterText(e->key())) {
				return true;
			}
		}
	}
	return false;
}

QString JumpDialog::text() const
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) {
		return ui->lineEdit_filter->text();
	} else {
		auto *item = ui->tableWidget->item(row, 0);
		if (item) {
			return item->text();
		}
	}
	return QString();
}

void JumpDialog::sort(NamedCommitList *items)
{
	std::sort(items->begin(), items->end(), [](NamedCommitItem const &l, NamedCommitItem const &r){
		auto Compare = [](NamedCommitItem const &l, NamedCommitItem const &r){
			if (l.type < r.type) return -1;
			if (l.type > r.type) return 1;
			if (l.type == NamedCommitItem::Type::BranchRemote) {
				int i = QString::fromStdString(l.remote).compare(QString::fromStdString(r.remote), Qt::CaseInsensitive);
				if (i != 0) return i;
			}
			return QString::fromStdString(l.name).compare(QString::fromStdString(r.name), Qt::CaseInsensitive);
		};
		return Compare(l, r) < 0;
	});
}

CommitRecord const *JumpDialog::findCommit(std::string const &id) const
{
	auto it = m->commit_record_map.find(id);
	if (it != m->commit_record_map.end()) {
		return it->second;
	}
	return nullptr;
}

NamedCommitItem const *JumpDialog::currentItem() const
{
	NamedCommitList const *list = m->filtered_items ? &*m->filtered_items : &m->all_items;
	int i = ui->tableWidget->currentRow();
	if (i < 0) {
		i = 0;
	}
	if (i < list->size()) {
		return &list->at(i);
	}
	return nullptr;
}

void JumpDialog::updateTable()
{
	auto InternalUpdateTable = [&](NamedCommitList const &list) {
		ui->tableWidget->clearContents();
		ui->tableWidget->setRowCount(list.size());
		for (int i = 0; i < list.size(); i++) {
			CommitRecord const *r = findCommit(list[i].id.toString());
			for (int col = 0; col < m->header.size(); col++) {
				auto *item = new QTableWidgetItem();
				QString text;
				switch (col)    {
				case 0: // Name
					text = QString::fromStdString(list[i].name);
					break;
				case 1: // Date
					if (r) {
						text = r->datetime;
					}
					break;
				case 2: // Message
					if (r) {
						text = r->message;
					}
					break;
				}
				item->setText(text);
				ui->tableWidget->setItem(i, col, item);
			}
		}
		ui->tableWidget->resizeColumnsToContents();
		ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	};

	m->filtered_items.reset();

	if (m->filter_text.isEmpty()) {
		InternalUpdateTable(m->all_items);
	} else {
		m->filtered_items = NamedCommitList();
		QStringList filter = misc::splitWords(m->filter_text);
		for (NamedCommitItem const &item: m->all_items) {
			auto Match = [&](QString  const &name){
				for (QString const &s : filter) {
					if (name.indexOf(s, 0, Qt::CaseInsensitive) < 0) {
						return false;
					}
				}
				return true;
			};
			if (!Match(QString::fromStdString(item.name))) continue;
			m->filtered_items->push_back(item);
		}
		InternalUpdateTable(*m->filtered_items);
	}
}

void JumpDialog::on_lineEdit_filter_textChanged(QString const &text)
{
	m->filter_text = text;
	updateTable();
}

void JumpDialog::on_tableWidget_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	int row = ui->tableWidget->currentRow();
	QTableWidgetItem *p = ui->tableWidget->item(row, 0);
	m->selected_name = p ? p->text() : QString();
}

void JumpDialog::on_pushButton_checkout_clicked()
{
	NamedCommitItem const *item = currentItem();
	if (item) {
		std::string name = item->name;
		if (item->type == NamedCommitItem::Type::BranchRemote) {
			if (misc::starts_with(name, "remotes/")) {
				size_t i = 8;
				i = name.find('/', i);
				if (i != std::string::npos && i > 0) {
					name = name.substr(i + 1);
				}
			}
		} else if (item->type == NamedCommitItem::Type::Tag) {
			if (misc::starts_with(name, "tags/")) {
				name = name.substr(5);
			}
		}
		if (mainwindow()->checkoutLocalBranch(name)) {
			done(QDialog::Accepted);
		}
	}
}


