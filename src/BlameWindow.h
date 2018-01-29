#ifndef BLAMEWINDOW_H
#define BLAMEWINDOW_H

#include <QDateTime>
#include <QDialog>

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
	QList<BlameItem> list_;
public:
	explicit BlameWindow(QWidget *parent, QString const &filename, QList<BlameItem> const &list);
	~BlameWindow();

	static QList<BlameItem> parseBlame(const char *begin, const char *end);
private:
	Ui::BlameWindow *ui;
};

#endif // BLAMEWINDOW_H
