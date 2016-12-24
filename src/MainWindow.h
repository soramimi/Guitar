#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Git.h"
#include "RepositoryData.h"
#include <memory>
#include <functional>

namespace Ui {
class MainWindow;
}

class QListWidgetItem;
class QTableWidgetItem;

class CommitList;

class MainWindow : public QMainWindow
{
	Q_OBJECT
private:

	struct Private;
	Private *pv;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	QPixmap const &digitsPixmap() const;

	QString currentWorkingCopyDir() const;

	static QString makeRepositoryName(QString const &loc);
	const Git::CommitItemList *logs() const;
	QColor color(unsigned int i);
	QList<Git::Branch> branchForCommit(const QString &id);
private slots:
	void on_action_add_all_triggered();
	void on_action_branch_checkout_triggered();
	void on_action_branch_merge_triggered();
	void on_action_branch_new_triggered();
	void on_action_clone_triggered();
	void on_action_commit_triggered();
	void on_action_config_global_credential_helper_triggered();
	void on_action_edit_git_config_triggered();
	void on_action_edit_gitignore_triggered();
	void on_action_edit_global_gitconfig_triggered();
	void on_action_edit_settings_triggered();
	void on_action_fetch_triggered();
	void on_action_open_existing_working_copy_triggered();
	void on_action_pull_triggered();
	void on_action_push_triggered();
	void on_action_set_remote_origin_url_triggered();
	void on_action_test_triggered();
	void on_action_view_refresh_triggered();
	void on_listWidget_files_currentRowChanged(int currentRow);
	void on_listWidget_repos_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_repos_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_staged_customContextMenuRequested(const QPoint &pos);
	void on_listWidget_unstaged_currentRowChanged(int currentRow);
	void on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos);
	void on_tableWidget_log_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_toolButton_commit_clicked();
	void on_toolButton_pull_clicked();
	void on_toolButton_push_clicked();
	void on_toolButton_select_all_clicked();
	void on_toolButton_stage_clicked();
	void on_toolButton_unstage_clicked();
	void onDiffWidgetResized();
	void onDiffWidgetWheelScroll(int lines);
	void onScrollValueChanged(int);
	void onScrollValueChanged2(int);
	void on_action_about_triggered();

	void on_toolButton_clone_clicked();

	void on_toolButton_fetch_clicked();

	void on_comboBox_filter_currentTextChanged(const QString &arg1);

	void on_toolButton_erase_filter_clicked();

	void on_tableWidget_log_customContextMenuRequested(const QPoint &pos);

private:
	Ui::MainWindow *ui;

	void makeDiff(QString const &old_id, QString const &new_id); // obsolete
	void updateFilesList(QString const &old_id, QString const &new_id, bool singlelist);
	void updateHeadFilesList(bool single);
	void updateRepositoriesList();
	QString getBookmarksFilePath() const;
	bool saveBookmarks() const;

	typedef std::shared_ptr<Git> GitPtr;
	GitPtr git(const QString &dir);
	GitPtr git();
	void openRepository(bool waitcursor = true);
	void openSelectedRepository();
	bool askAreYouSureYouWantToRun(const QString &title, const QString &command);
	void revertFile(const QStringList &path);
	void revertAllFiles();
	void prepareLogTableWidget();
	QStringList selectedStagedFiles() const;
	QStringList selectedUnstagedFiles() const;
	void for_each_selected_staged_files(std::function<void (const QString &)> fn);
	void for_each_selected_unstaged_files(std::function<void (const QString &)> fn);
	bool editFile(const QString &path, const QString &title);
	void updateCommitTree();
	void changeLog(QListWidgetItem *item, bool uncommited);
	void doUpdateFilesList();
	void updateSliderCursor();
	void checkGitCommand();
	void showFileList(bool signle);
	void clearFileList();
	void clearLog();
	void clearRepositoryInfo();
	int repositoryIndex(QListWidgetItem *item);
	QString diff_(const QString &old_id, const QString &new_id); // obsolete
	void makeDiff2(Git *g, const QString &id, QList<Git::Diff> *out);
	void udpateButton();
public:

	bool selectGitCommand();

	void setGitCommand(const QString &path, bool save);
	bool saveBlobAs(const QString &id, const QString &path);
	QString currentRepositoryName() const;
	Git::Branch currentBranch() const;
	bool isThereUncommitedChanges() const;
	bool saveFileAs(QString const &srcpath, QString const &dstpath);
	QString defaultWorkingDir() const;
protected:
	void resizeEvent(QResizeEvent *);

	// QObject interface
protected:
	void timerEvent(QTimerEvent *event);
protected slots:

	// QObject interface
public:
	bool event(QEvent *event);

	// QObject interface
public:
	bool eventFilter(QObject *watched, QEvent *event);
	void autoOpenRepository(QString dir);
	void saveRepositoryBookmark(const RepositoryItem &item);
	void drawDigit(QPainter *pr, int x, int y, int n) const;
	int digitWidth() const;
	int digitHeight() const;
};

#endif // MAINWINDOW_H
