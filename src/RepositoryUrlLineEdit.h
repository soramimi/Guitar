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
protected:
	void customEvent(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
public:
	explicit RepositoryUrlLineEdit(QWidget *parent = nullptr);
	~RepositoryUrlLineEdit();
private slots:
	void setNextRepositoryUrlCandidate(bool forward);
};

#endif // REPOSITORYURLLINEEDIT_H
