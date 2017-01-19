#ifndef FILEDIFFWIDGET_H
#define FILEDIFFWIDGET_H

#include <QDialog>
#include "Git.h"
#include "MainWindow.h"

namespace Ui {
class FileDiffWidget;
}

enum class ViewType {
	None,
	Left,
	Right
};

struct TextDiffLine {
	enum Type {
		Unknown,
		Unchanged,
		Add,
		Del,
	} type = Unknown;
	int hunk_number = -1;
	int line_number = -1;
	QString mark;
	QString line;
	TextDiffLine()
	{
	}
	TextDiffLine(QString const &text)
		: line(text)
	{
	}
};


class QTableWidgetItem;

class FileDiffWidget : public QWidget
{
	Q_OBJECT
public:
	struct DiffData {
		QStringList original_lines;
		QList<TextDiffLine> left_lines;
		QList<TextDiffLine> right_lines;
		QString path;
		QString left_id;
		QString right_id;
	};

	struct DrawData {
		int v_scroll_pos = 0;
		int h_scroll_pos = 0;
		int char_width = 0;
		int line_height = 0;
		QColor bgcolor_text;
		QColor bgcolor_add;
		QColor bgcolor_del;
		QColor bgcolor_add_dark;
		QColor bgcolor_del_dark;
		QColor bgcolor_gray;
		ViewType forcus = ViewType::None;
		DrawData();
	};
private:
	Ui::FileDiffWidget *ui;
	struct Private;
	Private *pv;

	FileDiffWidget::DiffData *diffdata();
	FileDiffWidget::DiffData const *diffdata() const;
	FileDiffWidget::DrawData *drawdata();
	FileDiffWidget::DrawData const *drawdata() const;

	int totalTextLines() const
	{
		return diffdata()->left_lines.size();
	}

	int fileviewScrollPos() const
	{
		return drawdata()->v_scroll_pos;
	}

	int visibleLines() const
	{
		int n = 0;
		if (drawdata()->line_height > 0) {
			n = fileviewHeight() / drawdata()->line_height;
			if (n < 1) n = 1;
		}
		return n;
	}

	void scrollTo(int value);

	void resetScrollBarValue();
	void updateVerticalScrollBar();
	void updateHorizontalScrollBar();
	void updateSliderCursor();
	void updateControls();

	int fileviewHeight() const;
	QString formatLine(QString const &text);

	void setDiffText(const QList<TextDiffLine> &left, const QList<TextDiffLine> &right);

	void setDataAsNewFile(const QByteArray &ba, const Git::Diff &diff);
	void setTextDiffData(const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);

	void init_diff_data_(const Git::Diff &diff);

	GitPtr git();

	bool isValidID_(const QString &id);
public:
	explicit FileDiffWidget(QWidget *parent = 0);
	~FileDiffWidget();

	void bind(MainWindow *mw);

	void clearDiffView();

	void updateDiffView(const Git::Diff &info, bool uncommited);
	void updateDiffView(QString id_left, QString id_right);

	QPixmap makeDiffPixmap(ViewType side, int width, int height, const DiffData *diffdata, const FileDiffWidget::DrawData *drawdata);

private slots:
	void onVerticalScrollValueChanged(int value);
	void onHorizontalScrollValueChanged(int value);
	void onDiffWidgetWheelScroll(int lines);
	void onScrollValueChanged2(int value);
	void onDiffWidgetResized();
protected:
	bool eventFilter(QObject *watched, QEvent *event);
};

#endif // FILEDIFFWIDGET_H
