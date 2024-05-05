#include "DropDownListFrame.h"
#include "RepositoryUrlLineEdit.h"
#include "common/misc.h"
#include <QKeyEvent>
#include <QDebug>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace {

class MyEvent : public QEvent {
public:
	int key = 0;
	MyEvent(int key)
		: QEvent(QEvent::User)
		, key(key)
	{
	}
};

} // namespace

struct RepositoryUrlLineEdit::Private {
	QString original_location;
	QStringList url_candidates;
	DropDownListFrame *drop_down_list = nullptr;
};

RepositoryUrlLineEdit::RepositoryUrlLineEdit(QWidget *parent)
	: QLineEdit{parent}
	, m{new Private}
{
	installEventFilter(this);
	
	connect(this, &QLineEdit::textChanged, [this](const QString &text) {
		m->original_location = text;
	});
	
	m->drop_down_list = new DropDownListFrame(this);
	connect(m->drop_down_list, &DropDownListFrame::itemClicked, [this](const QString &item) {
		setText(item);
	});
	connect(m->drop_down_list, &DropDownListFrame::done, [this]() {
		m->drop_down_list->close();
		qApp->postEvent(this, new QFocusEvent(QEvent::FocusIn));
	});
}

RepositoryUrlLineEdit::~RepositoryUrlLineEdit()
{
	delete m;
}

/**
 * @brief RepositoryUrlLineEdit::updateRepositoryUrlCandidates
 * @return
 *
 * テキストからリポジトリのURLを推測し、候補を作成する
 */
void RepositoryUrlLineEdit::updateRepositoryUrlCandidates()
{
	QString loc = m->original_location;
	m->url_candidates.clear();

	bool github = true;
	bool gitlab = true;
	QString username;
	QString reponame;
	const QString github_https = "https://github.com/";
	const QString github_git = "git@github.com:";
	const QString gitlab_https = "https://gitlab.com/";
	const QString gitlab_git = "git@gitlab.com:";
	if (loc.startsWith(github_https)) {
		QString s = loc.mid(github_https.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (loc.startsWith(github_git)) {
		QString s = loc.mid(github_git.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (loc.startsWith(gitlab_https)) {
		QString s = loc.mid(gitlab_https.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (loc.startsWith(gitlab_git)) {
		QString s = loc.mid(gitlab_git.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else {
		m->original_location = loc;
		QStringList s = misc::splitWords(loc);
		if (s.size() == 2) {
			username = s[0];
			reponame = s[1];
		} else if (s.size() == 3) {
			if (s[0] == "github") {
				github = true;
				gitlab = false;
			} else if (s[0] == "gitlab") {
				github = false;
				gitlab = true;
			}
			username = s[1];
			reponame = s[2];
		}
	}
	if (reponame.endsWith(".git")) {
		reponame = reponame.mid(0, reponame.size() - 4);
	}

	if (!username.isEmpty() && !reponame.isEmpty()) {
		if (github) {
			m->url_candidates.push_back(github_https + username + '/' + reponame + ".git");
			m->url_candidates.push_back(github_git + username + '/' + reponame + ".git");
		}
		if (gitlab) {
			m->url_candidates.push_back(gitlab_https + username + '/' + reponame + ".git");
			m->url_candidates.push_back(gitlab_git + username + '/' + reponame + ".git");
		}
		if (github && gitlab) {
			m->original_location = username + ' ' + reponame;
		}
		auto it = std::find(m->url_candidates.begin(), m->url_candidates.end(), m->original_location);
		if (it == m->url_candidates.end()) {
			m->url_candidates.push_back(loc);
		}
	}
}

/**
 * @brief RepositoryUrlLineEdit::setNextRepositoryUrlCandidate
 *
 * 次の候補を選択する
 */
void RepositoryUrlLineEdit::setNextRepositoryUrlCandidate(bool forward)
{
	updateRepositoryUrlCandidates();
	QString url = text();
	for (int i = 0; i < m->url_candidates.size(); i++) {
		if (m->url_candidates[i] == url) {
			if (forward) {
				i = (i + 1) % m->url_candidates.size();
			} else {
				i = (i + m->url_candidates.size() - 1) % m->url_candidates.size();
			}
			bool b = blockSignals(true);
			setText(m->url_candidates[i]);
			blockSignals(b);
			break;
		}
	}
}

void RepositoryUrlLineEdit::customEvent(QEvent *event)
{
	if (event->type() == QEvent::User) {
		MyEvent *e = static_cast<MyEvent *>(event);
		if (e->key == Qt::Key_Space) {
			setNextRepositoryUrlCandidate(true);
			return;
		}
		if (e->key == Qt::Key_Down) {
			updateRepositoryUrlCandidates();
			m->drop_down_list->setItems(m->url_candidates);
			m->drop_down_list->show();
			return;
		}
		return;
	}
}

bool RepositoryUrlLineEdit::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *>(event);
		if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
			QApplication::postEvent(this, new MyEvent(e->key()));
			return true;
		}
		if (e->key() == Qt::Key_Down) {
			QApplication::postEvent(this, new MyEvent(e->key()));
			return true;
		}
	}
	return QLineEdit::eventFilter(watched, event);
}

