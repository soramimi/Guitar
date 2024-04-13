#ifndef REPOSITORYURLCOMBOBOX_H
#define REPOSITORYURLCOMBOBOX_H

#include <QComboBox>
#include <QTimer>

class RepositoryUrlComboBox : public QComboBox {
	Q_OBJECT
private:
	QTimer timer_;
	QStringList url_candidates_;
	void makeRepositoryUrlCandidates(const QString &text);
public:
	explicit RepositoryUrlComboBox(QWidget *parent = nullptr);

signals:


	// QObject interface
public:
	bool eventFilter(QObject *watched, QEvent *event);
	QString text() const;
	void setText(QString text);
private slots:
	void setNextRepositoryUrlCandidate();
};

#endif // REPOSITORYURLCOMBOBOX_H
