#ifndef COMMITLOGTABLEWIDGET_H
#define COMMITLOGTABLEWIDGET_H

#include <QTableWidget>

class MainWindow;
class CommitLogTableWidget;

class CommitLogTableModel : public QAbstractItemModel {
public:
	struct Record {
		bool bold = false;
		QString commit_id;
		QString datetime;
		QString author;
		QString message;
		QString tooltip;
	};
	static QString escapeTooltipText(QString tooltip);
private:
	std::vector<Record> records_;
	CommitLogTableWidget *tablewidget();
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
	void setRecords(std::vector<Record> &&records);
};

/**
 * @brief コミットログテーブルウィジェット
 */
class CommitLogTableWidget : public QTableWidget {
	Q_OBJECT
	friend class CommitLogTableWidgetDelegate;
private:
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow() { return mainwindow_; }
	CommitLogTableModel *model_ = nullptr; // TODO:
public:
	explicit CommitLogTableWidget(QWidget *parent = nullptr);
	void setup(MainWindow *frame);
	void prepare();
	void setRecords(std::vector<CommitLogTableModel::Record> &&records)
	{
		model_->setRecords(std::move(records));
	}
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *e) override;
protected slots:
	void verticalScrollbarValueChanged(int value) override;
};

#endif // COMMITLOGTABLEWIDGET_H
