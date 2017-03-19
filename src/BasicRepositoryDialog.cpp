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
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();
	pv->mainwindow = mainwindow;
	pv->git = g;
}

BasicRepositoryDialog::~BasicRepositoryDialog()
{
	delete pv;
}

MainWindow *BasicRepositoryDialog::mainwindow()
{
	return pv->mainwindow;
}

GitPtr BasicRepositoryDialog::git()
{
	return pv->git;
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
