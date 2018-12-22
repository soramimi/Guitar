#ifndef REPOSITORYINFOFRAME_H
#define REPOSITORYINFOFRAME_H

#include <QFrame>

class RepositoryInfoFrame : public QFrame {
	Q_OBJECT
protected:
	void paintEvent(QPaintEvent *event) override;
public:
	explicit RepositoryInfoFrame(QWidget *parent = nullptr);
};

#endif // REPOSITORYINFOFRAME_H
