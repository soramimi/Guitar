#include "BasicRepositoryDialog.h"
#include "MainWindow.h"
#include <QTableWidget>
#include <QHeaderView>

struct BasicRepositoryDialog::Private {
	MainWindow *mainwindow = nullptr;
	GitPtr git;
};

BasicRepositoryDialog::BasicRepositoryDialog(MainWindow *mainwindow, GitPtr g)
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

MainWindow *BasicRepositoryDialog::mainwindow()
{
	return m->mainwindow;
}

GitPtr BasicRepositoryDialog::git()
{
	return m->git;
}

QString BasicRepositoryDialog::updateRemotesTable(QTableWidget *tablewidget)
{
	QList<Git::Remote> vec;
	GitPtr g = git();
	if (g->isValidWorkingCopy()) {
		g->getRemoteURLs(&vec);
	}
	QString url;
	QString alturl;
	int rows = vec.size();
	tablewidget->setColumnCount(3);
	tablewidget->setRowCount(rows);
	auto newQTableWidgetItem = [](QString const &text){
		QTableWidgetItem *item = new QTableWidgetItem();
		item->setText(text);
		return item;
	};
	auto SetHeaderItem = [&](int col, QString const &text){
		tablewidget->setHorizontalHeaderItem(col, newQTableWidgetItem(text));
	};
	SetHeaderItem(0, tr("Purpose"));
	SetHeaderItem(1, tr("Remote"));
	SetHeaderItem(2, tr("URL"));
	for (int row = 0; row < rows; row++) {
		Git::Remote const &r = vec[row];
		if (r.purpose == "push") {
			url = r.url;
		} else {
			alturl = r.url;
		}
		auto SetItem = [&](int col, QString const &text){
			tablewidget->setItem(row, col, newQTableWidgetItem(text));
			tablewidget->setRowHeight(col, 24);
		};
		SetItem(0, r.purpose);
		SetItem(1, r.remote);
		SetItem(2, r.url);
	}
	tablewidget->horizontalHeader()->setStretchLastSection(true);

	if (url.isEmpty()) {
		url = alturl;
	}
	return url;
}
