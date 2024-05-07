#include "BranchLabel.h"
#include "ApplicationGlobal.h"

QColor BranchLabel::color(Type type)
{

	switch (type) {
	case Head:         return global->appsettings.branch_label_color.head;
	case LocalBranch:  return global->appsettings.branch_label_color.local;
	case RemoteBranch: return global->appsettings.branch_label_color.remote;
	case Tag:          return global->appsettings.branch_label_color.tag;
	}
	return QColor(224, 224, 224); // gray
}
