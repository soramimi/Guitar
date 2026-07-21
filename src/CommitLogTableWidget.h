#ifndef COMMITLOGTABLEWIDGET_H
#define COMMITLOGTABLEWIDGET_H

#include "../subprojects/IncrementalSearchPlugin/src/IncrementalSearchPlugin.h"
#include "CommitRecord.h"
#include "IncrementalSearchHelper.h"
#include "RepositoryTreeWidget.h"
#include <QTableWidget>
#include <memory>

struct GitCommitItem;
class MainWindow;
class CommitLogTableWidget;

class CommitLogTableModel : public QAbstractItemModel {
	friend class CommitLogTableWidgetDelegate;
	friend class CommitLogTableWidget;
public:
	static QString escapeTooltipText(QString tooltip);
private:
	std::span<CommitRecord const *const> records_;
	std::vector<size_t> index_;
	std::string filter_text_;
	IncrementalSearchFilter incremental_search_filter_;
	IncrementalSearchFilter const &getIncrementalSearchFilter() const
	{
		return incremental_search_filter_;
	}
	CommitLogTableWidget *tablewidget();
	const CommitRecord *record(int row) const;
	const CommitRecord *record(QModelIndex const &index) const;
	int rowcount() const;
	void private_SetFilter(const std::string &text);
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
	void setRecords(std::span<CommitRecord const *const> records);
	bool setFilter(const std::string &text);
	bool isFiltered() const
	{
		return !filter_text_.empty();
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
	const GitCommitItem &commitItem(int row) const;
public:
	explicit CommitLogTableWidget(QWidget *parent = nullptr);
	void setup(MainWindow *frame);
	void setRecords(std::span<CommitRecord const *const> records);
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
	void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
public:
	int rowCount() const;
	int currentRow() const;
	void setCurrentCell(int row, int col);
	int actualLogIndex() const;
	QRect visualItemRect(int row, int col);
	void setFilter(const std::string &filter);
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
