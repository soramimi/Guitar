#ifndef COMMITVIEWWINDOW_H
#define COMMITVIEWWINDOW_H

#include "Git.h"

#include <QDialog>

class MainWindow;

namespace Ui {
class CommitViewWindow;
}

class CommitViewWindow : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	static MainWindow *mainwindow();
public:
	explicit CommitViewWindow(MainWindow *parent, Git::CommitItem const *commit);
	~CommitViewWindow() override;

private slots:
	void on_listWidget_files_currentRowChanged(int currentRow);

	void on_listWidget_files_customContextMenuRequested(const QPoint &pos);

private:
	Ui::CommitViewWindow *ui;
};

#endif // COMMITVIEWWINDOW_H
