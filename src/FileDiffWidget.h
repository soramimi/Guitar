#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include <QScrollBar>
#include <QWidget>
#include "Git.h"

class FileDiffSliderWidget;

class FileDiffWidget : public QWidget
{
	Q_OBJECT
private:
	struct Line;

	struct Private;
	Private *pv;
	QString formatLine(const QString &text, bool diffmode);

	class HunkItem {
	public:
		int hunk_number = -1;
		size_t pos, len;
		QStringList lines;
	};

	void setText_(QList<Line> const &left, QList<Line> const &right, bool diffmode);
public:
	explicit FileDiffWidget(QWidget *parent);
	~FileDiffWidget();

	void updateVerticalScrollBar(QScrollBar *sb);

	void clear();

	int lineCount() const;
	void scrollTo(int value);
	void setData(const QByteArray &ba, const Git::Diff &diff, bool uncmmited, const QString &workingdir);

	enum class Side {
		None,
		Left,
		Right
	};

	QPixmap makePixmap(Side side, int width, int height);
	int totalLines() const;
	int visibleLines() const;
	int scrollPos() const;
	void setDataAsNewFile(const QByteArray &ba);
protected:
	void paintEvent(QPaintEvent *);
	void wheelEvent(QWheelEvent *);
	void resizeEvent(QResizeEvent *);
signals:
	void scrollByWheel(int lines);
	void resized();


	// QWidget interface
protected:
	void contextMenuEvent(QContextMenuEvent *);
};

#endif // FILEDIFFWIDGET_H
