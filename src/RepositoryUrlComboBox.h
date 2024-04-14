#ifndef REPOSITORYURLCOMBOBOX_H
#define REPOSITORYURLCOMBOBOX_H

#include <QComboBox>
#include <QTimer>

class RepositoryUrlComboBox : public QComboBox {
	Q_OBJECT
private:
	QStringList url_candidates_;
	void makeRepositoryUrlCandidates(const QString &text);
protected:
	void customEvent(QEvent *event);
	bool eventFilter(QObject *watched, QEvent *event);
public:
	explicit RepositoryUrlComboBox(QWidget *parent = nullptr);
	QString text() const;
	void setText(QString text);
private slots:
	void setNextRepositoryUrlCandidate();
};

#endif // REPOSITORYURLCOMBOBOX_H
