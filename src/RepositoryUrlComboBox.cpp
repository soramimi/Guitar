#include "RepositoryUrlComboBox.h"
#include "common/misc.h"
#include <QKeyEvent>
#include <QDebug>

RepositoryUrlComboBox::RepositoryUrlComboBox(QWidget *parent)
	: QComboBox{parent}
{
	installEventFilter(this);
	setEditable(true);

	connect(this, &QComboBox::editTextChanged, [this](const QString &text) {
		setText(text);
	});

	connect(&timer_, &QTimer::timeout, [this]() {
		bool b = blockSignals(true);
		setNextRepositoryUrlCandidate();
		blockSignals(b);
	});
}

/**
 * @brief RepositoryUrlComboBox::makeRepositoryUrlCandidates
 * @return
 *
 * テキストからリポジトリのURLを推測し、候補を作成する
 */
void RepositoryUrlComboBox::makeRepositoryUrlCandidates(QString const &text)
{
	url_candidates_ = {};

	QString github_user;
	QString github_repo;
	QString github_https = "https://github.com/";
	QString github_git = "git@github.com:";
	if (text.startsWith(github_https)) {
		QString s = text.mid(github_https.size());
		int i = s.indexOf('/');
		github_user = s.left(i);
		github_repo = s.mid(i + 1);
	} else if (text.startsWith(github_git)) {
		QString s = text.mid(github_git.size());
		int i = s.indexOf('/');
		github_user = s.left(i);
		github_repo = s.mid(i + 1);
	} else {
		QStringList s = misc::splitWords(text);
		if (s.size() == 2 && s[0] != "github") {
			github_user = s[0];
			github_repo = s[1];
		} else if (s.size() == 3 && s[0] == "github") {
			github_user = s[1];
			github_repo = s[2];
		}
	}
	if (github_repo.endsWith(".git")) {
		github_repo = github_repo.mid(0, github_repo.size() - 4);
	}

	if (!github_user.isEmpty() && !github_repo.isEmpty()) {
		url_candidates_.push_back(github_https + github_user + '/' + github_repo + ".git");
		url_candidates_.push_back(github_git + github_user + '/' + github_repo + ".git");
		url_candidates_.push_back(github_user + ' ' + github_repo);
	}
}

/**
 * @brief RepositoryUrlComboBox::setNextRepositoryUrlCandidate
 *
 * 次の候補を選択する
 */
void RepositoryUrlComboBox::setNextRepositoryUrlCandidate()
{
	QString url = currentText();
	for (int i = 0; url_candidates_.size(); i++) {
		if (url_candidates_[i] == url) {
			i = (i + 1) % url_candidates_.size();
			setText(url_candidates_[i]);
			break;
		}
	}
}

/**
 * @brief RepositoryUrlComboBox::text
 * @return
 *
 * 編集中のテキストを返す
 */
QString RepositoryUrlComboBox::text() const
{
	return currentText();
}

/**
 * @brief RepositoryUrlComboBox::setText
 * @param text
 *
 * テキストをセットする
 * この関数は、テキストからリポジトリのURLを推測し、候補を表示する
 * また、候補の中から次の候補を選択する
 * textはurl_candidates_の要素を指していることがあるため参照で受け取ることはできない
 */
void RepositoryUrlComboBox::setText(QString text)
{
	makeRepositoryUrlCandidates(text);
	bool b = blockSignals(true);
	clear();
	for (QString const &s : url_candidates_) {
		addItem(s);
	}
	setEditText(text);
	blockSignals(b);
}

/**
 * @brief RepositoryUrlComboBox::eventFilter
 * @param watched
 * @param event
 * @return
 *
 * キーボードイベントをフィルタリングする
 * Ctrl + Spaceが押された場合、次の候補を選択する
 */
bool RepositoryUrlComboBox::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *>(event);
		if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
			timer_.setSingleShot(true);
			timer_.start(10);
			event->accept();
			return true;
		}
	}
	return QComboBox::eventFilter(watched, event);
}
