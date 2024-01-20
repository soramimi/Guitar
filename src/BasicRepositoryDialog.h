#ifndef BASICREPOSITORYDIALOG_H
#define BASICREPOSITORYDIALOG_H

#include <QDialog>
#include "Git.h"

class QTableWidget;
class MainWindow;

class BasicRepositoryDialog : public QDialog {
public:
	explicit BasicRepositoryDialog(MainWindow *mainwindow, GitPtr g);
	~BasicRepositoryDialog() override;
private:
	struct Private;
	Private *m;
protected:
	MainWindow *mainwindow();

	GitPtr git();
	QString updateRemotesTable(QTableWidget *tablewidget);

	const QList<Git::Remote> *remotes() const;
	void getRemotes_();
	void setSshKey_(const QString &sshkey);
};

#endif // BASICREPOSITORYDIALOG_H
