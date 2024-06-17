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
protected:
	void paintEvent(QPaintEvent *event);
public:
	ProgressTextLabel(QWidget *parent = nullptr);
	void setProgress(float progress);
};

#endif // PROGRESSTEXTLABEL_H
