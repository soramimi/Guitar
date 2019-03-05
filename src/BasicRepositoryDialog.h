#ifndef BASICREPOSITORYDIALOG_H
#define BASICREPOSITORYDIALOG_H

#include <QDialog>
#include "Git.h"

class QTableWidget;
class BasicMainWindow;

class BasicRepositoryDialog : public QDialog {
public:
	explicit BasicRepositoryDialog(BasicMainWindow *mainwindow, const GitPtr &g);
	 ~BasicRepositoryDialog() override;
private:
	struct Private;
	Private *m;
protected:
	BasicMainWindow *mainwindow();

	GitPtr git();
	QString updateRemotesTable(QTableWidget *tablewidget);

	const QList<Git::Remote> *remotes() const;
};

#endif // BASICREPOSITORYDIALOG_H
