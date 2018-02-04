#ifndef BLAMEWINDOW_H
#define BLAMEWINDOW_H

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

class BlameWindow : public QDialog
{
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit BlameWindow(MainWindow *parent, QString const &filename, QList<BlameItem> const &list);
	~BlameWindow();

	static QList<BlameItem> parseBlame(const char *begin, const char *end);
private slots:
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

private:
	Ui::BlameWindow *ui;
};

#endif // BLAMEWINDOW_H
