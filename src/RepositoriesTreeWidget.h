#ifndef REPOSITORIESTREEWIDGET_H
#define REPOSITORIESTREEWIDGET_H

#include <QTreeWidget>
#include "RepositoryData.h"

class MainWindow;
struct RepositoryData;

class RepositoriesTreeWidget : public QTreeWidget {
	Q_OBJECT
	friend class RepositoriesTreeWidgetItemDelegate;
public:
	enum class RepositoriesListStyle {
		_Keep,
		Standard,
		SortRecent,
	};
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow();
	QTreeWidgetItem *current_item = nullptr;
	QString getFilterText() const;
	RepositoriesListStyle current_repositories_list_style_ = RepositoriesListStyle::Standard;
protected:
	void paintEvent(QPaintEvent *event);
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
public:
	static QTreeWidgetItem *newQTreeWidgetItem();
	static QTreeWidgetItem *newQTreeWidgetFolderItem(QString const &name);
public:
	explicit RepositoriesTreeWidget(QWidget *parent = nullptr);
	~RepositoriesTreeWidget();
	void setFilterText(QString const &filtertext);
	void setRepositoriesListStyle(RepositoriesListStyle style);
	RepositoriesListStyle currentRepositoriesListStyle() const;
	void updateList(RepositoriesTreeWidget::RepositoriesListStyle style, const QList<RepositoryData> &repos, QString const &filter);
signals:
	void dropped();
};

#endif // REPOSITORIESTREEWIDGET_H
