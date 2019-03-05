#include "BasicRepositoryDialog.h"
#include "BasicMainWindow.h"
#include <QTableWidget>
#include <QHeaderView>

struct BasicRepositoryDialog::Private {
	BasicMainWindow *mainwindow = nullptr;
	GitPtr git;
	QList<Git::Remote> remotes;
};

BasicRepositoryDialog::BasicRepositoryDialog(BasicMainWindow *mainwindow, GitPtr const &g)
	: QDialog(mainwindow)
	, m(new Private)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->mainwindow = mainwindow;
	m->git = g;
}

BasicRepositoryDialog::~BasicRepositoryDialog()
{
	delete m;
}

BasicMainWindow *BasicRepositoryDialog::mainwindow()
{
	return m->mainwindow;
}

GitPtr BasicRepositoryDialog::git()
{
	return m->git;
}

QList<Git::Remote> const *BasicRepositoryDialog::remotes() const
{
	return &m->remotes;
}

QString BasicRepositoryDialog::updateRemotesTable(QTableWidget *tablewidget)
{
	tablewidget->clear();
	m->remotes.clear();
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		g->getRemoteURLs(&m->remotes);
	}
	QString url;
	QString alturl;
	int rows = m->remotes.size();
	tablewidget->setColumnCount(3);
	tablewidget->setRowCount(rows);
	auto newQTableWidgetItem = [](QString const &text){
		auto *item = new QTableWidgetItem;
		item->setSizeHint(QSize(20, 20));
		item->setText(text);
		return item;
	};
	auto SetHeaderItem = [&](int col, QString const &text){
		tablewidget->setHorizontalHeaderItem(col, newQTableWidgetItem(text));
	};
	SetHeaderItem(0, tr("Name"));
	SetHeaderItem(1, tr("Purpose"));
	SetHeaderItem(2, tr("URL"));
	for (int row = 0; row < rows; row++) {
		Git::Remote const &r = m->remotes[row];
		if (r.purpose == "push") {
			url = r.url;
		} else {
			alturl = r.url;
		}
		auto SetItem = [&](int col, QString const &text){
			tablewidget->setItem(row, col, newQTableWidgetItem(text));
		};
		SetItem(0, r.name);
		SetItem(1, r.purpose);
		SetItem(2, r.url);
	}
	tablewidget->horizontalHeader()->setStretchLastSection(true);

	if (url.isEmpty()) {
		url = alturl;
	}
	return url;
}
