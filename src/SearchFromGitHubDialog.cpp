#include "SearchFromGitHubDialog.h"
#include "ui_SearchFromGitHubDialog.h"

#include "webclient.h"
#include "misc.h"
#include "json.h"
#include "charvec.h"
#include "urlencode.h"
#include "TypeTraits.h"

#include <QDebug>
#include <QThread>
#include <QDesktopServices>
#include <QUrl>
#include <functional>

static QString toQString(std::string const &s)
{
	return QString::fromUtf8(s.c_str(), s.size());
}

SearchFromGitHubDialog::SearchFromGitHubDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SearchFromGitHubDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->tableWidget->setItemDelegate(&item_delegate);

	connect(ui->label_hyperlink, SIGNAL(clicked()), this, SLOT(onHyperlinkClicked()));

	ui->pushButton_ok->setEnabled(false);
}

SearchFromGitHubDialog::~SearchFromGitHubDialog()
{
	delete ui;
}

QString SearchFromGitHubDialog::url() const
{
	return url_;
}

class GitHubRequestThread : public QThread {
protected:
	void run()
	{
		ok = false;
		WebContext webcx;
		WebClient web(&webcx);
		if (web.get(WebClient::URL(url)) == 200) {
			WebClient::Response const &r = web.response();
			if (!r.content.empty()) {
				text = to_stdstr(r.content);
				ok = true;
			}
		}
	}
public:
	std::string url;
	bool ok = false;
	std::string text;
};

namespace{
	class SearchFromGitHubDialogAddItem {
	private:
		Ui::SearchFromGitHubDialog *ui_;
		size_t row_;
		std::reference_wrapper<int> col_;
	public:
		SearchFromGitHubDialogAddItem() = delete;
		SearchFromGitHubDialogAddItem(const SearchFromGitHubDialogAddItem&) = delete;
		SearchFromGitHubDialogAddItem(SearchFromGitHubDialogAddItem&&) = delete;
		SearchFromGitHubDialogAddItem& operator=(const SearchFromGitHubDialogAddItem&) = delete;
		SearchFromGitHubDialogAddItem& operator=(SearchFromGitHubDialogAddItem&&) = delete;
		SearchFromGitHubDialogAddItem(Ui::SearchFromGitHubDialog *ui, size_t row, int &col)
			: ui_(ui), row_(row), col_(col)
		{}
		template<
			typename Callback,
			typename std::enable_if<
				type_traits::is_callable<Callback(QTableWidgetItem*), void>::value,
				std::nullptr_t
			>::type = nullptr
		>
		void operator()(Callback&& callback)
		{
			auto p = new QTableWidgetItem();
			callback(p);
			this->ui_->tableWidget->setItem(this->row_, this->col_.get(), p);
			this->col_.get()++;
		}
	};
}
void SearchFromGitHubDialog::on_pushButton_search_clicked()
{
	std::string q = ui->lineEdit_keywords->text().trimmed().toStdString();
	q = url_encode(q);
	if (q.empty()) return;

	GitHubRequestThread th;
	{
		OverrideWaitCursor;
		th.url = "https://api.github.com/search/repositories?q=" + q;
		th.start();
		while (!th.wait(1)) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		th.wait();
	}
	if (!th.ok) return;

	items.clear();

	JSON json;
	json.parse(th.text);
	for (JSON::Node const &node1 : json.node.children) {
		if (node1.name == "items") {
			for (JSON::Node const &node2 : node1.children) {
				Item item;
				for (JSON::Node const &node3 : node2.children) {
					if (node3.name == "full_name") {
						item.full_name = node3.value;
					} else if (node3.name == "description") {
						item.description = node3.value;
					} else if (node3.name == "html_url") {
						item.html_url = node3.value;
					} else if (node3.name == "ssh_url") {
						item.ssh_url = node3.value;
					} else if (node3.name == "clone_url") {
						item.clone_url = node3.value;
					} else if (node3.name == "score") {
						item.score = strtod(node3.value.c_str(), nullptr);
					}
				}
				if (!item.full_name.empty()) {
					items.push_back(item);
				}
			}
		}
	}

	std::sort(items.begin(), items.end(), [](Item const &l, Item const &r){
		return l.score > r.score; // 降順
	});

	QStringList cols = {
		tr("Name"),
		tr("Owner"),
		tr("Score"),
		tr("Description"),
	};

	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(items.size());

	for (int col = 0; col < cols.size(); col++) {
		QTableWidgetItem *p = new QTableWidgetItem();
		p->setText(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, p);
	}

	for (size_t row = 0; row < items.size(); row++) {
		Item const &item = items[row];

		QString name = QString::fromStdString(item.full_name);
		QString owner;
		int i = name.indexOf('/');
		if (i > 0) {
			owner = name.mid(0, i);
			name = name.mid(i + 1);
		}

		int col = 0;
		SearchFromGitHubDialogAddItem AddItem(ui, row, std::ref(col));

		AddItem([&](QTableWidgetItem *p){
			p->setData(Qt::UserRole, (int)row);
			p->setText(name);
		});
		AddItem([&](QTableWidgetItem *p){
			p->setText(owner);
		});
		AddItem([&](QTableWidgetItem *p){
			char tmp[100];
			sprintf(tmp, "%.2f", item.score);
			p->setText(tmp);
			p->setTextAlignment(Qt::AlignRight);
		});
		AddItem([&](QTableWidgetItem *p){
			p->setText(toQString(item.description));
		});

		ui->tableWidget->setRowHeight(row, 24);
	}
	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

void SearchFromGitHubDialog::updateUI()
{
	auto Check = [&](){
		if (ui->radioButton_ssh->isChecked()) {
			url_ = ui->lineEdit_ssh->text();
			if (!url_.isEmpty()) {
				return true;
			}
		}
		if (ui->radioButton_http->isChecked()) {
			url_ = ui->lineEdit_http->text();
			if (!url_.isEmpty()) {
				return true;
			}
		}
		url_ = QString();
		return false;
	};
	ui->pushButton_ok->setEnabled(Check());
}

void SearchFromGitHubDialog::on_tableWidget_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	int row = ui->tableWidget->currentRow();
	QTableWidgetItem *p = ui->tableWidget->item(row, 0);
	if (p) {
		size_t i = p->data(Qt::UserRole).toUInt();
		if (i < items.size()) {
			Item const &item = items[i];
			ui->lineEdit_ssh->setText(toQString(item.ssh_url));
			ui->lineEdit_http->setText(toQString(item.clone_url));
			ui->label_hyperlink->setText(toQString(item.html_url));
		}
	}
	updateUI();
}

void SearchFromGitHubDialog::on_radioButton_ssh_clicked()
{
	updateUI();
}

void SearchFromGitHubDialog::on_radioButton_http_clicked()
{
	updateUI();
}

void SearchFromGitHubDialog::onHyperlinkClicked()
{
	QString url = ui->label_hyperlink->text();
	QDesktopServices::openUrl(QUrl(url));
}



