#ifndef REPOSITORYINFOFRAME_H
#define REPOSITORYINFOFRAME_H

#include <QFrame>

class RepositoryInfoFrame : public QFrame
{
	Q_OBJECT
public:
	explicit RepositoryInfoFrame(QWidget *parent = 0);

signals:

public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // REPOSITORYINFOFRAME_H
