#ifndef COMMITMESSAGEGENERATOR_H
#define COMMITMESSAGEGENERATOR_H

#include "Git.h"

class CommitMessageGenerator {
public:
	CommitMessageGenerator() = default;
	QStringList generate(GitPtr g);
};

#endif // COMMITMESSAGEGENERATOR_H
