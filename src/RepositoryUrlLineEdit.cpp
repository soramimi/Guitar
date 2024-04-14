#include "DropDownListFrame.h"
#include "RepositoryUrlLineEdit.h"
#include "common/misc.h"
#include <QKeyEvent>
#include <QDebug>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

class CtrlSpaceEvent : public QEvent {
public:
	bool forward_ = true;
	CtrlSpaceEvent(bool forward)
		: QEvent(QEvent::User)
		, forward_(forward)
	{
	}
};

struct RepositoryUrlLineEdit::Private {
	QStringList url_candidates;
	DropDownListFrame *drop_down_frame = nullptr;
	QRect drop_down_botton_rect;
};

RepositoryUrlLineEdit::RepositoryUrlLineEdit(QWidget *parent)
	: QLineEdit{parent}
	, m{new Private}
{
	m->drop_down_frame = new DropDownListFrame(this);

	installEventFilter(this);

	connect(m->drop_down_frame, &DropDownListFrame::itemClicked, [this](QString const &text) {
		setText(text);
		m->drop_down_frame->hide();
	});

	connect(this, &QLineEdit::textChanged, [this](const QString &text) {
		setText(text);
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
	QString url = text();
	m->url_candidates = {};

	bool github = true;
	bool gitlab = true;
	QString username;
	QString reponame;
	const QString github_https = "https://github.com/";
	const QString github_git = "git@github.com:";
	const QString gitlab_https = "https://gitlab.com/";
	const QString gitlab_git = "git@gitlab.com:";
	if (url.startsWith(github_https)) {
		QString s = url.mid(github_https.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (url.startsWith(github_git)) {
		QString s = url.mid(github_git.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (url.startsWith(gitlab_https)) {
		QString s = url.mid(gitlab_https.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else if (url.startsWith(gitlab_git)) {
		QString s = url.mid(gitlab_git.size());
		int i = s.indexOf('/');
		username = s.left(i);
		reponame = s.mid(i + 1);
	} else {
		QStringList s = misc::splitWords(url);
		if (s.size() == 2) {
			username = s[0];
			reponame = s[1];
		} else if (s.size() == 3) {
			if (s[0] == "github") {
				github = true;
			} else if (s[0] == "gitlab") {
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
		m->url_candidates.push_back(username + ' ' + reponame);
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
	for (int i = 0; m->url_candidates.size(); i++) {
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

/**
 * @brief RepositoryUrlLineEdit::showDropDown
 *
 * ドロップダウンリストを表示する
 */
void RepositoryUrlLineEdit::showDropDown()
{
	updateRepositoryUrlCandidates();
	m->drop_down_frame->setItems(m->url_candidates);
	m->drop_down_frame->show();
}

void RepositoryUrlLineEdit::paintEvent(QPaintEvent *event)
{
	const int w = width();
	const int h = height();
	m->drop_down_botton_rect = QRect(w - h, 0, h, h);

	QLineEdit::paintEvent(event);
	QPainter pr(this);
	QStyleOption opt;
	opt.initFrom(this);
	opt.rect = m->drop_down_botton_rect;
	qApp->style()->drawPrimitive(QStyle::PE_IndicatorButtonDropDown, &opt, &pr, this);
}

void RepositoryUrlLineEdit::mouseMoveEvent(QMouseEvent *event)
{
	QPoint p = event->pos();
	if (m->drop_down_botton_rect.contains(p)) {
		setCursor(Qt::ArrowCursor);
		event->accept();
		return;
	}
	QLineEdit::mouseMoveEvent(event);
}

void RepositoryUrlLineEdit::mousePressEvent(QMouseEvent *event)
{
	QPoint p = event->pos();
	if (m->drop_down_botton_rect.contains(p)) {
		showDropDown();
		event->accept();
		return;
	}
	QLineEdit::mousePressEvent(event);
}

void RepositoryUrlLineEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
	QPoint p = event->pos();
	if (m->drop_down_botton_rect.contains(p)) {
		showDropDown();
		event->accept();
		return;
	}
	QLineEdit::mouseDoubleClickEvent(event);
}

void RepositoryUrlLineEdit::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Down) {
		showDropDown();
	}
	QLineEdit::keyPressEvent(event);
}

void RepositoryUrlLineEdit::customEvent(QEvent *event)
{
	if (event->type() == QEvent::User) {
		CtrlSpaceEvent *e = static_cast<CtrlSpaceEvent *>(event);
		setNextRepositoryUrlCandidate(e->forward_);
	}
}

bool RepositoryUrlLineEdit::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *>(event);
		if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
			bool forward = !(e->modifiers() & Qt::ShiftModifier);
			QApplication::postEvent(this, new CtrlSpaceEvent(forward));
			return true;
		}
	}
	return QLineEdit::eventFilter(watched, event);
}

