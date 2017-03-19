#ifndef BASICREPOSITORYDIALOG_H
#define BASICREPOSITORYDIALOG_H

#include <QDialog>
#include "Git.h"

class QTableWidget;
class MainWindow;

class BasicRepositoryDialog : public QDialog {
public:
	explicit BasicRepositoryDialog(MainWindow *mainwindow, GitPtr g);
	virtual ~BasicRepositoryDialog();
private:
	struct Private;
	Private *pv;
protected:
	MainWindow *mainwindow();

	GitPtr git();
	QString updateRemotesTable(QTableWidget *tablewidget);

};

#endif // BASICREPOSITORYDIALOG_H
