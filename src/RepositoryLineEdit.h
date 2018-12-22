#ifndef REPOSITORYLINEEDIT_H
#define REPOSITORYLINEEDIT_H

#include <QLineEdit>

class RepositoryLineEdit : public QLineEdit {
	Q_OBJECT
protected:
	void dropEvent(QDropEvent *event) override;
public:
	explicit RepositoryLineEdit(QWidget *parent = nullptr);
};

#endif // REPOSITORYLINEEDIT_H
