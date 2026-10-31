#ifndef PTYPROCESSFACTORY_H
#define PTYPROCESSFACTORY_H

#include <memory>

class AbstractPtyProcess;

enum class PtyProcessBackend {
	Direct,
	Worker,
};

// On POSIX, both choices use the native PTY implementation.
std::shared_ptr<AbstractPtyProcess> createPtyProcess(PtyProcessBackend backend);

#endif // PTYPROCESSFACTORY_H
