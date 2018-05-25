#-------------------------------------------------
#
# Project created by QtCreator 2016-02-06T19:08:45
#
#-------------------------------------------------

QT       += core gui widgets svg network
win32:QT += winextras

TARGET = Guitar
TEMPLATE = app

CONFIG += c++11

win32:CONFIG += MSVC
#win32:CONFIG += MinGW

TRANSLATIONS = Guitar_ja.ts

DEFINES += APP_GUITAR

DEFINES += USE_DARK_THEME

DEFINES += HAVE_POSIX_OPENPT
macx:DEFINES += HAVE_SYS_TIME_H
macx:DEFINES += HAVE_UTMPX

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder
linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += @executable_path/../Frameworks

linux:QTPLUGIN += ibusplatforminputcontextplugin
#linux:QTPLUGIN += fcitxplatforminputcontextplugin

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/src/texteditor
win32:INCLUDEPATH += $$PWD/winpty
win32:LIBS += $$PWD/winpty/winpty.lib

# OpenSSL

linux:LIBS += -lssl -lcrypto
haiku:LIBS += -lssl -lcrypto -lnetwork
macx:INCLUDEPATH += /usr/local/include
macx:LIBS += /usr/local/lib/libssl.a /usr/local/lib/libcrypto.a

win32:MSVC {
	INCLUDEPATH += C:\openssl\include
	LIBS += -LC:\openssl\lib
}

win32:MinGW {
	INCLUDEPATH += C:\Qt\Tools\mingw530_32\opt\include
	LIBS += -LC:\Qt\Tools\mingw530_32\opt\lib
	LIBS += -lcrypto -lssl
}

# execute 'ruby prepare.rb' automatically

prepare.target = prepare
prepare.commands = cd $$PWD && ruby -W0 prepare.rb
QMAKE_EXTRA_TARGETS += prepare
PRE_TARGETDEPS += prepare


# zlib

win32:MSVC {
	CONFIG(debug, debug|release):LIBS += $$PWD/_lib/libz.lib
	CONFIG(release, debug|release):LIBS += $$PWD/_lib/libz.lib
}

win32:MinGW {
	CONFIG(debug, debug|release):LIBS += $$PWD/_lib/liblibz.a
	CONFIG(release, debug|release):LIBS += $$PWD/_lib/liblibz.a
}

!haiku {
    unix:CONFIG(debug, debug|release):LIBS += $$PWD/_lib/libzd.a
    unix:CONFIG(release, debug|release):LIBS += $$PWD/_lib/libz.a
	#unix:LIBS += -lz
}

haiku:LIBS += -lz

win32 {
	LIBS += -ladvapi32 -lshell32 -luser32 -lws2_32
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
	src/common/joinpath.cpp \
	src/common/misc.cpp \
	src/ConfigCredentialHelperDialog.cpp \
	src/MySettings.cpp \
	src/FileDiffWidget.cpp \
	src/TextEditDialog.cpp \
	src/LogTableWidget.cpp \
	src/FileDiffSliderWidget.cpp \
	src/FileUtil.cpp \
	src/SettingsDialog.cpp \
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
	src/RepositoriesTreeWidget.cpp \
	src/SelectCommandDialog.cpp \
	src/ImageViewWidget.cpp \
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
	src/SearchFromGitHubDialog.cpp \
	src/webclient.cpp \
	src/charvec.cpp \
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
	src/ExperimentDialog.cpp \
	src/gunzip.cpp \
	src/AvatarLoader.cpp \
	src/SettingNetworkForm.cpp \
	src/Photoshop.cpp \
	src/SettingBehaviorForm.cpp \
	src/MyProcess.cpp \
	src/FileViewWidget.cpp \
	src/MyTextEditorWidget.cpp \
	src/AbstractProcess.cpp \
	src/texteditor/AbstractCharacterBasedApplication.cpp \
	src/texteditor/InputMethodPopup.cpp \
	src/texteditor/TextEditorTheme.cpp \
	src/texteditor/TextEditorWidget.cpp \
	src/texteditor/unicode.cpp \
	src/texteditor/UnicodeWidth.cpp \
	src/MyImageViewWidget.cpp \
	src/SetGlobalUserDialog.cpp \
	src/ReflogWindow.cpp \
	src/LegacyWindowsStyleTreeControl.cpp \
	darktheme/src/DarkStyle.cpp \
	darktheme/src/NinePatch.cpp \
	src/Theme.cpp \
	src/ApplicationGlobal.cpp \
    src/BlameWindow.cpp \
    src/MenuButton.cpp \
    src/SettingGeneralForm.cpp \
    src/WelcomeWizardDialog.cpp \
    src/DialogHeaderFrame.cpp \
    src/CommitViewWindow.cpp \
    src/EditRemoteDialog.cpp \
    src/gpg.cpp \
    src/SelectGpgKeyDialog.cpp \
    src/SetGpgSigningDialog.cpp \
    src/CommitDialog.cpp \
    src/ConfigSigningDialog.cpp \
    src/AreYouSureYouWantToContinueConnectingDialog.cpp \
    src/LineEditDialog.cpp

HEADERS  += \
	src/MainWindow.h \
	src/Git.h \
	src/common/joinpath.h \
	src/common/misc.h \
	src/ConfigCredentialHelperDialog.h \
	src/MySettings.h \
	src/main.h \
	src/FileDiffWidget.h \
	src/TextEditDialog.h \
	src/LogTableWidget.h \
	src/FileDiffSliderWidget.h \
	src/FileUtil.h \
	src/SettingsDialog.h \
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
	src/RepositoriesTreeWidget.h \
	src/SelectCommandDialog.h \
	src/ImageViewWidget.h \
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
	src/SearchFromGitHubDialog.h \
	src/webclient.h \
	src/charvec.h \
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
	src/ExperimentDialog.h \
	src/gunzip.h \
	src/AvatarLoader.h \
	src/SettingNetworkForm.h \
	src/Photoshop.h \
	src/SettingBehaviorForm.h \
	src/MyProcess.h \
	src/MyTextEditorWidget.h \
	src/AbstractProcess.h \
	src/texteditor/AbstractCharacterBasedApplication.h \
	src/texteditor/InputMethodPopup.h \
	src/texteditor/TextEditorTheme.h \
	src/texteditor/TextEditorWidget.h \
	src/texteditor/unicode.h \
	src/texteditor/UnicodeWidth.h \
	src/MyImageViewWidget.h \
	src/SetGlobalUserDialog.h \
	src/ReflogWindow.h \
	src/LegacyWindowsStyleTreeControl.h \
	darktheme/src/DarkStyle.h \
	darktheme/src/NinePatch.h \
	src/Theme.h \
	src/ApplicationGlobal.h \
    src/BlameWindow.h \
    src/MenuButton.h \
    src/SettingGeneralForm.h \
    src/WelcomeWizardDialog.h \
    src/DialogHeaderFrame.h \
    src/CommitViewWindow.h \
    src/EditRemoteDialog.h \
    src/gpg.h \
    src/SelectGpgKeyDialog.h \
    src/SetGpgSigningDialog.h \
    src/CommitDialog.h \
    src/ConfigSigningDialog.h \
    src/AreYouSureYouWantToContinueConnectingDialog.h \
    src/LineEditDialog.h

FORMS    += \
	src/MainWindow.ui \
	src/ConfigCredentialHelperDialog.ui \
	src/TextEditDialog.ui \
	src/SettingsDialog.ui \
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
	src/SearchFromGitHubDialog.ui \
	src/JumpDialog.ui \
	src/CheckoutDialog.ui \
	src/DeleteBranchDialog.ui \
	src/PushDialog.ui \
	src/SettingDirectoriesForm.ui \
	src/SettingExampleForm.ui \
	src/CreateRepositoryDialog.ui \
	src/ExperimentDialog.ui \
	src/SettingNetworkForm.ui \
	src/SettingBehaviorForm.ui \
    src/SetGlobalUserDialog.ui \
    src/ReflogWindow.ui \
    src/BlameWindow.ui \
    src/SettingGeneralForm.ui \
    src/WelcomeWizardDialog.ui \
    src/CommitViewWindow.ui \
    src/EditRemoteDialog.ui \
    src/SelectGpgKeyDialog.ui \
    src/SetGpgSigningDialog.ui \
    src/CommitDialog.ui \
    src/ConfigSigningDialog.ui \
    src/AreYouSureYouWantToContinueConnectingDialog.ui \
    src/LineEditDialog.ui

RESOURCES += \
    resources.qrc

unix {
	SOURCES += \
		src/unix/UnixProcess.cpp \
		src/unix/UnixPtyProcess.cpp
	HEADERS += \
		src/unix/UnixProcess.h \
		src/unix/UnixPtyProcess.h
}

win32 {
	SOURCES += \
		src/win32/thread.cpp \
		src/win32/event.cpp \
        src/win32/win32.cpp \
		src/win32/Win32Process.cpp \
		src/win32/Win32PtyProcess.cpp

	HEADERS += \
		src/win32/thread.h \
		src/win32/event.h \
		src/win32/mutex.h \
        src/win32/win32.h \
		src/win32/Win32Process.h \
		src/win32/Win32PtyProcess.h
}
