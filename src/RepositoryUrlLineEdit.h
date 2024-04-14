#ifndef REPOSITORYURLLINEEDIT_H
#define REPOSITORYURLLINEEDIT_H

#include <QLineEdit>
#include <QTimer>

class QListWidget;

class RepositoryUrlLineEdit : public QLineEdit {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void updateRepositoryUrlCandidates();
	void showDropDown();
protected:
	void keyPressEvent(QKeyEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void paintEvent(QPaintEvent *event);
	void customEvent(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
public:
	explicit RepositoryUrlLineEdit(QWidget *parent = nullptr);
	~RepositoryUrlLineEdit();
private slots:
	void setNextRepositoryUrlCandidate(bool forward);
};

#endif // REPOSITORYURLLINEEDIT_H
