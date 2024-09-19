#ifndef PROGRESSTEXTLABEL_H
#define PROGRESSTEXTLABEL_H

#include <QLabel>
#include <QTimer>

class ProgressTextLabel : public QLabel {
	Q_OBJECT
private:
	float progress_ = 0;
	QTimer timer_;
	int animation_ = 0;
	bool bar_visible = true;
	bool msg_visible = true;
protected:
	void paintEvent(QPaintEvent *event);
public:
	ProgressTextLabel(QWidget *parent = nullptr);
	void setElementVisible(bool bar, bool msg);
	void setProgress(float progress);
};

#endif // PROGRESSTEXTLABEL_H
