#ifndef REPOSITORYLINEEDIT_H
#define REPOSITORYLINEEDIT_H

#include <QLineEdit>

class RepositoryLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	explicit RepositoryLineEdit(QWidget *parent = 0);

signals:

public slots:

	// QWidget interface
protected:
	void dropEvent(QDropEvent *event);
};

#endif // REPOSITORYLINEEDIT_H
