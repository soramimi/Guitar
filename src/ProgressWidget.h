#ifndef PROGRESSWIDGET_H
#define PROGRESSWIDGET_H

#include <QWidget>

namespace Ui {
class ProgressWidget;
}

class ProgressWidget : public QWidget {
	Q_OBJECT
private:
	Ui::ProgressWidget *ui;

public:
	explicit ProgressWidget(QWidget *parent = nullptr);
	~ProgressWidget();
	void setProgress(float progress);
	void setText(QString const &text);
	void clear();
};

#endif // PROGRESSWIDGET_H
