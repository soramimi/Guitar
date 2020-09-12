#ifndef BRANCHLABEL_H
#define BRANCHLABEL_H

#include <QColor>
#include "ApplicationGlobal.h"


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

	static QColor color(Type type)
	{

		switch (type) {
		case Head:         return global->appsettings.branch_label_color.head;
		case LocalBranch:  return global->appsettings.branch_label_color.local;
		case RemoteBranch: return global->appsettings.branch_label_color.remote;
		case Tag:          return global->appsettings.branch_label_color.tag;
		}
		return QColor(224, 224, 224); // gray
	}
};

#endif // BRANCHLABEL_H
