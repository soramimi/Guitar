#ifndef BRANCHLABEL_H
#define BRANCHLABEL_H

#include <QColor>
// #include "ApplicationGlobal.h"


/**
 * @brief ログテーブルウィジェットのブランチ名ラベル
 */
class BranchLabel {
public:
	enum Kind {
		Head,
		LocalBranch,
		RemoteBranch,
		Tag,
	};
	Kind kind;
	QString text;
	QString info;
	BranchLabel(Kind kind = LocalBranch)
		: kind(kind)
	{
	}

	static QColor color(Kind type);
};

#endif // BRANCHLABEL_H
