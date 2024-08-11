#ifndef BLAMEWINDOW_H
#define BLAMEWINDOW_H

#include "Git.h"

#include <QDateTime>
#include <QDialog>

class MainWindow;
class QTableWidgetItem;


namespace Ui {
class BlameWindow;
}

struct BlameItem {
	QString commit_id;
	QString author;
	QDateTime time;
	int line_number = 0;
	QString text;
};

class BlameWindow : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit BlameWindow(MainWindow *parent, QString const &filename, QList<BlameItem> const &list);
	~BlameWindow() override;

	static QList<BlameItem> parseBlame(char const *begin, char const *end);
private slots:
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
private:
	Ui::BlameWindow *ui;
	MainWindow *mainwindow();

	// QObject interface
    Git::CommitID getCommitId(QTableWidgetItem *item) const;
	Git::CommitID currentCommitId() const;
};

#endif // BLAMEWINDOW_H
