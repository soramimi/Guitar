#-------------------------------------------------
#
# Project created by QtCreator 2016-02-06T19:08:45
#
#-------------------------------------------------

QT       += core gui widgets svg

TARGET = Guitar
TEMPLATE = app

CONFIG += c++11

unix:QMAKE_CXXFLAGS += -Werror=return-type -Werror=trigraphs

linux:QTPLUGIN += ibusplatforminputcontextplugin

INCLUDEPATH += $$PWD/src

#INCLUDEPATH += $$PWD/../libgit2/include

#win32:Debug:LIBS += $$PWD/../_build_libgit2/debug/libgit2.lib
#win32:Release:LIBS += $$PWD/../_build_libgit2/release/libgit2.lib

#unix:debug:LIBS += $$PWD/../_build_libgit2_Debug/liblibgit2.a
#unix:release:LIBS += $$PWD/../_build_libgit2_Release/liblibgit2.a

win32:LIBS += advapi32.lib

macx {
	QMAKE_INFO_PLIST = Info.plist
	ICON += Guitar.icns
	t.path=Contents/Resources
	QMAKE_BUNDLE_DATA += t
}

SOURCES += \
	src/main.cpp\
	src/MainWindow.cpp \
	src/Git.cpp \
	src/joinpath.cpp \
	src/misc.cpp \
	src/PushDialog.cpp \
	src/ConfigCredentialHelperDialog.cpp \
	src/MySettings.cpp \
	src/win32.cpp \
	src/FileDiffWidget.cpp \
	src/TextEditDialog.cpp \
	src/LogTableWidget.cpp \
    src/FileDiffSliderWidget.cpp \
    src/LibGit2.cpp \
    src/FileUtil.cpp \
    src/SelectGitCommandDialog.cpp \
    src/SettingsDialog.cpp \
    src/NewBranchDialog.cpp \
    src/CheckoutBranchDialog.cpp \
    src/MergeBranchDialog.cpp \
    src/CloneDialog.cpp \
    src/AboutDialog.cpp \
    src/RepositoryInfoFrame.cpp \
    src/RepositoryPropertyDialog.cpp \
    src/RepositoryData.cpp \
    src/MyToolButton.cpp \
    src/GitDiff.cpp \
    src/CommitPropertyDialog.cpp \
    src/Terminal.cpp

HEADERS  += \
	src/MainWindow.h \
	src/Git.h \
	src/joinpath.h \
	src/misc.h \
	src/PushDialog.h \
	src/ConfigCredentialHelperDialog.h \
	src/MySettings.h \
	src/win32.h \
	src/main.h \
	src/FileDiffWidget.h \
	src/TextEditDialog.h \
	src/LogTableWidget.h \
    src/FileDiffSliderWidget.h \
    src/LibGit2.h \
    src/FileUtil.h \
    src/SelectGitCommandDialog.h \
    src/SettingsDialog.h \
    src/NewBranchDialog.h \
    src/CheckoutBranchDialog.h \
    src/MergeBranchDialog.h \
    src/CloneDialog.h \
    src/AboutDialog.h \
    src/RepositoryInfoFrame.h \
    src/RepositoryPropertyDialog.h \
    src/RepositoryData.h \
    src/MyToolButton.h \
    src/GitDiff.h \
    src/CommitPropertyDialog.h \
    src/Terminal.h

FORMS    += \
	src/MainWindow.ui \
	src/PushDialog.ui \
	src/ConfigCredentialHelperDialog.ui \
	src/TextEditDialog.ui \
    src/SelectGitCommandDialog.ui \
    src/SettingsDialog.ui \
    src/NewBranchDialog.ui \
    src/CheckoutBranchDialog.ui \
    src/MergeBranchDialog.ui \
    src/CloneDialog.ui \
    src/AboutDialog.ui \
    src/RepositoryPropertyDialog.ui \
    src/CommitPropertyDialog.ui

RESOURCES += \
    resources.qrc
