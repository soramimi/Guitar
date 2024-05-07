#ifndef BRANCHLABEL_H
#define BRANCHLABEL_H

#include <QColor>
// #include "ApplicationGlobal.h"


/**
 * @brief ログテーブルウィジェットのブランチ名ラベル
 */
class BranchLabel {
public:
	enum Type {
		Head,
		LocalBranch,
		RemoteBranch,
		Tag,
	};
	Type kind;
	QString text;
	QString info;
	BranchLabel(Type kind = LocalBranch)
		: kind(kind)
	{
	}

	static QColor color(Type type);
};

#endif // BRANCHLABEL_H
