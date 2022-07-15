#ifndef REPOSITORYLINEEDIT_H
#define REPOSITORYLINEEDIT_H

#include <QLineEdit>

class RepositoryLineEdit : public QLineEdit {
	Q_OBJECT
protected:
	void dropEvent(QDropEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event);
public:
	explicit RepositoryLineEdit(QWidget *parent = nullptr);
};

#endif // REPOSITORYLINEEDIT_H
