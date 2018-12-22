#ifndef PLATFORM_H
#define PLATFORM_H

#include <QApplication>

#ifdef Q_OS_WIN
#include "win32/win32.h"
#else
#include <unistd.h>
#endif

#ifdef Q_OS_MAC
extern "C" char **environ;
#endif

#endif // PLATFORM_H
