#-------------------------------------------------
#
# Project created by QtCreator 2016-02-06T19:08:45
#
#-------------------------------------------------

QT       += core gui widgets svg network

TARGET = Guitar
TEMPLATE = app

CONFIG += c++11

TRANSLATIONS = Guitar_ja.ts

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder
linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += @executable_path/../Frameworks

linux:QTPLUGIN += ibusplatforminputcontextplugin
#linux:QTPLUGIN += fcitxplatforminputcontextplugin

INCLUDEPATH += $$PWD/src

# OpenSSL

linux:LIBS += -lssl -lcrypto
macx:INCLUDEPATH += /usr/local/include
macx:LIBS += /usr/local/lib/libssl.a /usr/local/lib/libcrypto.a
win32:INCLUDEPATH += C:\openssl\include
win32:LIBS += -LC:\openssl\lib

# execute 'ruby prepare.rb' automatically

prepare.target = prepare
prepare.commands = cd $$PWD && ruby -W0 prepare.rb
QMAKE_EXTRA_TARGETS += prepare
PRE_TARGETDEPS += prepare


# libgit2

#INCLUDEPATH += $$PWD/../libgit2/include

#win32:Debug:LIBS += $$PWD/../_build_libgit2/debug/libgit2.lib
#win32:Release:LIBS += $$PWD/../_build_libgit2/release/libgit2.lib

#unix:debug:LIBS += $$PWD/../_build_libgit2_Debug/liblibgit2.a
#unix:release:LIBS += $$PWD/../_build_libgit2_Release/liblibgit2.a


# zlib

win32:Debug:LIBS += $$PWD/../_build_zlib/debug/libz.lib
win32:Release:LIBS += $$PWD/../_build_zlib/release/libz.lib

unix:debug:LIBS += $$PWD/../_build_zlib_Debug/libz.a
unix:release:LIBS += $$PWD/../_build_zlib_Release/libz.a

#unix:LIBS += -lz


win32 {
	LIBS += advapi32.lib
	RC_FILE = win.rc
	QMAKE_SUBSYSTEM_SUFFIX=,5.01
}

macx {
	QMAKE_INFO_PLIST = Info.plist
	ICON += Guitar.icns
	t.path=Contents/Resources
	QMAKE_BUNDLE_DATA += t
}

SOURCES += \
	version.c \
	src/main.cpp\
	src/MainWindow.cpp \
	src/Git.cpp \
	src/joinpath.cpp \
	src/misc.cpp \
	src/ConfigCredentialHelperDialog.cpp \
	src/MySettings.cpp \
	src/FileDiffWidget.cpp \
	src/TextEditDialog.cpp \
	src/LogTableWidget.cpp \
    src/FileDiffSliderWidget.cpp \
    src/LibGit2.cpp \
    src/FileUtil.cpp \
    src/SettingsDialog.cpp \
    src/NewBranchDialog.cpp \
    src/MergeBranchDialog.cpp \
    src/CloneDialog.cpp \
    src/AboutDialog.cpp \
    src/RepositoryInfoFrame.cpp \
    src/RepositoryPropertyDialog.cpp \
    src/RepositoryData.cpp \
    src/MyToolButton.cpp \
    src/GitDiff.cpp \
    src/CommitPropertyDialog.cpp \
    src/Terminal.cpp \
    src/EditTagDialog.cpp \
    src/DeleteTagsDialog.cpp \
    src/LegacyWindowsStyleTreeControl.cpp \
    src/RepositoriesTreeWidget.cpp \
    src/SelectCommandDialog.cpp \
    src/FilePreviewWidget.cpp \
    src/FileHistoryWindow.cpp \
    src/GitPackIdxV2.cpp \
    src/GitPack.cpp \
    src/GitObjectManager.cpp \
    src/FilePropertyDialog.cpp \
    src/BigDiffWindow.cpp \
    src/MaximizeButton.cpp \
    src/CommitExploreWindow.cpp \
    src/ReadOnlyLineEdit.cpp \
    src/ReadOnlyPlainTextEdit.cpp \
    src/MyTableWidgetDelegate.cpp \
    src/SetRemoteUrlDialog.cpp \
    src/ClearButton.cpp \
    src/SetUserDialog.cpp \
	src/ProgressDialog.cpp \
    src/LogWidget.cpp \
    src/SearchFromGitHubDialog.cpp \
    src/webclient.cpp \
    src/charvec.cpp \
    src/json.cpp \
    src/urlencode.cpp \
    src/HyperLinkLabel.cpp \
    src/JumpDialog.cpp \
    src/CheckoutDialog.cpp \
    src/DeleteBranchDialog.cpp \
	src/BasicRepositoryDialog.cpp \
    src/RemoteRepositoriesTableWidget.cpp \
    src/LocalSocketReader.cpp \
    src/PushDialog.cpp \
    src/StatusLabel.cpp \
    src/RepositoryLineEdit.cpp \
    src/DirectoryLineEdit.cpp \
	src/SettingDirectoriesForm.cpp \
    src/AbstractSettingForm.cpp \
    src/SettingExampleForm.cpp \
    src/CreateRepositoryDialog.cpp \
    src/GitHubAPI.cpp \
    src/MemoryReader.cpp \
    src/ExperimentDialog.cpp

HEADERS  += \
	src/MainWindow.h \
	src/Git.h \
	src/joinpath.h \
	src/misc.h \
	src/ConfigCredentialHelperDialog.h \
	src/MySettings.h \
	src/main.h \
	src/FileDiffWidget.h \
	src/TextEditDialog.h \
	src/LogTableWidget.h \
    src/FileDiffSliderWidget.h \
    src/LibGit2.h \
    src/FileUtil.h \
    src/SettingsDialog.h \
    src/NewBranchDialog.h \
    src/MergeBranchDialog.h \
    src/CloneDialog.h \
    src/AboutDialog.h \
    src/RepositoryInfoFrame.h \
    src/RepositoryPropertyDialog.h \
    src/RepositoryData.h \
    src/MyToolButton.h \
    src/GitDiff.h \
    src/CommitPropertyDialog.h \
    src/Terminal.h \
    src/EditTagDialog.h \
    src/DeleteTagsDialog.h \
    src/LegacyWindowsStyleTreeControl.h \
    src/RepositoriesTreeWidget.h \
    src/SelectCommandDialog.h \
    src/FilePreviewWidget.h \
    src/FileHistoryWindow.h \
    src/Debug.h \
    src/GitPackIdxV2.h \
    src/GitPack.h \
    src/GitObjectManager.h \
    src/FilePropertyDialog.h \
    src/BigDiffWindow.h \
    src/MaximizeButton.h \
    src/CommitExploreWindow.h \
    src/ReadOnlyLineEdit.h \
    src/ReadOnlyPlainTextEdit.h \
    src/MyTableWidgetDelegate.h \
    src/SetRemoteUrlDialog.h \
    myzlib.h \
    src/ClearButton.h \
    src/SetUserDialog.h \
	src/ProgressDialog.h \
    src/LogWidget.h \
    src/SearchFromGitHubDialog.h \
    src/webclient.h \
    src/charvec.h \
    src/json.h \
    src/urlencode.h \
    src/HyperLinkLabel.h \
    src/JumpDialog.h \
    src/CheckoutDialog.h \
    src/DeleteBranchDialog.h \
	src/BasicRepositoryDialog.h \
    src/RemoteRepositoriesTableWidget.h \
    src/LocalSocketReader.h \
    src/PushDialog.h \
    src/StatusLabel.h \
    src/RepositoryLineEdit.h \
    src/DirectoryLineEdit.h \
	src/SettingDirectoriesForm.h \
    src/AbstractSettingForm.h \
    src/SettingExampleForm.h \
    src/CreateRepositoryDialog.h \
    src/GitHubAPI.h \
    src/MemoryReader.h \
    src/ExperimentDialog.h

FORMS    += \
	src/MainWindow.ui \
	src/ConfigCredentialHelperDialog.ui \
	src/TextEditDialog.ui \
    src/SettingsDialog.ui \
    src/NewBranchDialog.ui \
    src/MergeBranchDialog.ui \
    src/CloneDialog.ui \
    src/AboutDialog.ui \
    src/RepositoryPropertyDialog.ui \
    src/CommitPropertyDialog.ui \
    src/EditTagDialog.ui \
    src/DeleteTagsDialog.ui \
    src/SelectCommandDialog.ui \
	src/FileDiffWidget.ui \
    src/FileHistoryWindow.ui \
    src/FilePropertyDialog.ui \
    src/BigDiffWindow.ui \
    src/CommitExploreWindow.ui \
    src/SetRemoteUrlDialog.ui \
    src/SetUserDialog.ui \
    src/ProgressDialog.ui \
    src/SearchFromGitHubDialog.ui \
    src/JumpDialog.ui \
    src/CheckoutDialog.ui \
    src/DeleteBranchDialog.ui \
    src/PushDialog.ui \
	src/SettingDirectoriesForm.ui \
    src/SettingExampleForm.ui \
    src/CreateRepositoryDialog.ui \
    src/ExperimentDialog.ui

RESOURCES += \
    resources.qrc



win32 {
	SOURCES += \
		src/win32/thread.cpp \
		src/win32/event.cpp \
		src/win32/win32.cpp

	HEADERS  += \
		src/win32/thread.h \
		src/win32/event.h \
		src/win32/mutex.h \
		src/win32/win32.h
}
