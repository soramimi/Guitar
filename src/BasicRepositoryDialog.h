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
	Private *m;
protected:
	MainWindow *mainwindow();

	GitPtr git();
	QString updateRemotesTable(QTableWidget *tablewidget);

	const QList<Git::Remote> *remotes() const;
};

#endif // BASICREPOSITORYDIALOG_H
