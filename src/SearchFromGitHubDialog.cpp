#include "SearchFromGitHubDialog.h"
#include "ui_SearchFromGitHubDialog.h"
#include "BasicMainWindow.h"
#include "common/misc.h"
#include "urlencode.h"
#include <QDebug>
#include <QDesktopServices>
#include <QThread>
#include <QUrl>
#include <functional>

using SearchResultItem = GitHubAPI::SearchResultItem;

static QString toQString(std::string const &s)
{
	return QString::fromUtf8(s.c_str(), s.size());
}

SearchFromGitHubDialog::SearchFromGitHubDialog(QWidget *parent, BasicMainWindow *mw)
	: QDialog(parent)
	, ui(new Ui::SearchFromGitHubDialog)
	, mainwindow(mw)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->tableWidget->setItemDelegate(&item_delegate);

	connect(ui->label_hyperlink, &HyperLinkLabel::clicked, this, &SearchFromGitHubDialog::onHyperlinkClicked);

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

void SearchFromGitHubDialog::on_pushButton_search_clicked()
{
	std::string q = ui->lineEdit_keywords->text().trimmed().toStdString();
	q = url_encode(q);
	if (q.empty()) return;

	GitHubAPI github(mainwindow);
	items = github.searchRepository(q);

	QStringList cols = {
		tr("Name"),
		tr("Owner"),
		tr("Score"),
		tr("Description"),
	};

	ui->tableWidget->setColumnCount(cols.size());
	ui->tableWidget->setRowCount(items.size());

	for (int col = 0; col < cols.size(); col++) {
		auto *p = new QTableWidgetItem();
		p->setText(cols[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, p);
	}

	for (int row = 0; row < items.size(); row++) {
		SearchResultItem const &item = items[row];
		QTableWidgetItem *p;

		QString name = QString::fromStdString(item.full_name);
		QString owner;
		int i = name.indexOf('/');
		if (i > 0) {
			owner = name.mid(0, i);
			name = name.mid(i + 1);
		}

		int col = 0;
		auto AddItem = [&](std::function<void(QTableWidgetItem *)> callback){
			p = new QTableWidgetItem();
			callback(p);
			ui->tableWidget->setItem(row, col, p);
			col++;
		};

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
			p->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
		int i = p->data(Qt::UserRole).toInt();
		if (i < items.size()) {
			SearchResultItem const &item = items[i];
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


