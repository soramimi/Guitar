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

#ifdef Q_OS_WIN
#define GIT_COMMAND "git.exe"
#define FILE_COMMAND "file.exe"
#define GPG_COMMAND "gpg.exe"
#define GPG2_COMMAND "gpg2.exe"
#define SSH_COMMAND "ssh.exe"
#else
#define GIT_COMMAND "git"
#define FILE_COMMAND "file"
#define GPG_COMMAND "gpg"
#define GPG2_COMMAND "gpg2"
#define SSH_COMMAND "ssh"
#endif

#endif // PLATFORM_H
