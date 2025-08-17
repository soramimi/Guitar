#ifndef COMMITLOGTABLEWIDGET_H
#define COMMITLOGTABLEWIDGET_H

#include "Git.h"


#include "RepositoryTreeWidget.h"

#include <QTableWidget>

class MainWindow;
class CommitLogTableWidget;

struct CommitRecord {
	bool bold = false;
	QString commit_id;
	QString datetime;
	QString author;
	QString message;
	QString tooltip;
};
Q_DECLARE_METATYPE(CommitRecord)

class CommitLogTableModel : public QAbstractItemModel {
	friend class CommitLogTableWidgetDelegate;
	friend class CommitLogTableWidget;
public:
	static QString escapeTooltipText(QString tooltip);
private:
	std::vector<CommitRecord> records_;
	std::vector<size_t> index_;
	QString filter_text_;
	MigemoFilter filter_;
	CommitLogTableWidget *tablewidget();
	CommitRecord const &record(int row) const;
	CommitRecord const &record(QModelIndex const &index) const;
	int rowcount() const;
	void privateSetFilter(const QString &text);
public:
	CommitLogTableModel(QObject *parent = nullptr)
		: QAbstractItemModel(parent)
	{
	}
	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	void setRecords(std::vector<CommitRecord> &&records);
	bool setFilter(const QString &text);
	bool isFiltered() const
	{
		return !filter_text_.isEmpty();
	}
	int unfilteredIndex(int i) const;
};

/**
 * @brief コミットログテーブルウィジェット
 */
class CommitLogTableWidget : public QTableView {
	Q_OBJECT
	friend class CommitLogTableWidgetDelegate;
private:
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow() { return mainwindow_; }
	MainWindow const *mainwindow() const { return mainwindow_; }
	CommitLogTableModel *model_ = nullptr; // TODO:
	// MigemoFilter filter_;
	const GitCommitItem &commitItem(int row) const;
public:
	explicit CommitLogTableWidget(QWidget *parent = nullptr);
	void setup(MainWindow *frame);
	void setRecords(std::vector<CommitRecord> &&records);
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
	void currentChanged(const QModelIndex &current, const QModelIndex &previous);
public:
	int rowCount() const;
	int currentRow() const;
	void setCurrentCell(int row, int col);
	int actualLogIndex() const;
	QRect visualItemRect(int row, int col);
	void setFilter(const QString &filter);
	void adjustAppearance();
	void updateViewport();
	int unfilteredIndex(int i) const
	{
		return model_->unfilteredIndex(i);
	}
	void setCurrentRow(int row);
signals:
	void currentRowChanged(int row);
};

#endif // COMMITLOGTABLEWIDGET_H
