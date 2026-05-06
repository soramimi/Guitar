
QMAKE_PROJECT_DEPTH = 0

QT += core gui widgets svg network

msvc:lessThan(QT_MAJOR_VERSION, 6) {
	QT += winextras
}

TARGET = Guitar
TEMPLATE = app

CPP_STD = c++17

CONFIG += $$CPP_STD nostrip debug_info static

# CONFIG += unsafe ### don't enable
unsafe {
	DEFINES += UNSAFE_ENABLED
	msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib
	LIBS += -lssh
}

msvc:DEFINES += NOMINMAX

INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

SRC = $$PWD/src

TRANSLATIONS = $$SRC/resources/translations/Guitar_ja.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_ru.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_es.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_ta.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_zh-CN.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_zh-TW.ts

DEFINES += APP_GUITAR

DEFINES += HAVE_POSIX_OPENPT
macx:DEFINES += HAVE_SYS_TIME_H
macx:DEFINES += HAVE_UTMPX

gcc:QMAKE_CXXFLAGS += -std=$$CPP_STD -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter
linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += @executable_path/../Frameworks


INCLUDEPATH += $$SRC
INCLUDEPATH += $$SRC/common
INCLUDEPATH += $$SRC/coloredit
INCLUDEPATH += $$SRC/texteditor
INCLUDEPATH += $$PWD/IncrementalSearch/mecab/mecab/src/
INCLUDEPATH += $$PWD/filetype/src/

msvc:INCLUDEPATH += $$PWD/misc/winpty/include
msvc:LIBS += $$PWD/misc/winpty/x64/lib/winpty.lib -lshlwapi

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

# OpenSSL

linux {
	static_link_openssl {
		LIBS += $$OPENSSL_LIB_DIR/libssl.a $$OPENSSL_LIB_DIR/libcrypto.a -ldl
	} else {
		LIBS += -lssl -lcrypto
	}
}
haiku:LIBS += -lssl -lcrypto -lnetwork
macx:INCLUDEPATH += /opt/homebrew/include
macx:LIBS += /opt/homebrew/lib/libssl.a /opt/homebrew/lib/libcrypto.a
msvc:LIBS += -llibcrypto -llibssl

# network library

# CONFIG += use_libcurl
use_libcurl {
	DEFINES += USE_LIBCURL
	!msvc:LIBS += -lcurl
	msvc:LIBS += -llibcurl
}

# incremental search

# execute 'ruby prepare.rb' automatically

prepare.target = prepare
prepare.commands = cd $$PWD && ruby -W0 prepare.rb
QMAKE_EXTRA_TARGETS += prepare
PRE_TARGETDEPS += prepare

# zlib

msvc {
	LIBS += -lzlib
}

!msvc {
	LIBS += -lz
}

#

msvc:CONFIG(release, debug|release):LIBS += $$PWD/filetype/lib/filetype.lib $$PWD/filetype/lib/file.lib $$PWD/filetype/lib/oniguruma.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/filetype/lib/filetyped.lib $$PWD/filetype/lib/filed.lib $$PWD/filetype/lib/onigurumad.lib
!msvc:CONFIG(release, debug|release):LIBS += $$PWD/filetype/lib/libfiletype.a $$PWD/filetype/lib/libfile.a $$PWD/filetype/lib/liboniguruma.a
!msvc:CONFIG(debug, debug|release):LIBS += $$PWD/filetype/lib/libfiletyped.a $$PWD/filetype/lib/libfiled.a $$PWD/filetype/lib/libonigurumad.a

# experimental: MeCaSearch

INCLUDEPATH += IncrementalSearch/

!msvc:LIBS += $$PWD/IncrementalSearch/libmecasearch.a
msvc:CONFIG(release, debug|release):LIBS += $$PWD/IncrementalSearch/mecasearch.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/IncrementalSearch/mecasearchd.lib

#

win32 {
	LIBS += -ladvapi32 -lshell32 -luser32 -lws2_32
	RC_FILE = win.rc
	QMAKE_SUBSYSTEM_SUFFIX=,5.01
}

macx {
	QMAKE_INFO_PLIST = Info.plist
	ICON += src/resources/Guitar.icns
	macres.files = $$ICON
	macres.path = Contents/Resources
	QMAKE_BUNDLE_DATA += macres
}

SOURCES += \
	$$SRC/FileTypeDetector.cpp \
	$$SRC/process/ProcessExample.cpp \
	$$SRC/common/q/DateTime.cpp \
	$$SRC/common/q/DirIterator.cpp \
	$$SRC/common/qmisc.cpp \
	$$SRC/AboutDialog.cpp \
	$$SRC/AbstractGitSession.cpp \
	$$SRC/AbstractProcess.cpp \
	$$SRC/AbstractSettingForm.cpp \
	$$SRC/AddRepositoriesCollectivelyDialog.cpp \
	$$SRC/AddRepositoryDialog.cpp \
	$$SRC/ApplicationGlobal.cpp \
	$$SRC/ApplicationSettings.cpp \
	$$SRC/AreYouSureYouWantToContinueConnectingDialog.cpp \
	$$SRC/AvatarLoader.cpp \
	$$SRC/BigDiffWindow.cpp \
	$$SRC/BlameWindow.cpp \
	$$SRC/BlockSignals.cpp \
	$$SRC/BranchLabel.cpp \
	$$SRC/CheckoutDialog.cpp \
	$$SRC/CherryPickDialog.cpp \
	$$SRC/CleanSubModuleDialog.cpp \
	$$SRC/ClearButton.cpp \
	$$SRC/CloneFromGitHubDialog.cpp \
	$$SRC/ColorButton.cpp \
	$$SRC/CommitDetailGetter.cpp \
	$$SRC/CommitDialog.cpp \
	$$SRC/CommitExploreWindow.cpp \
	$$SRC/CommitLogTableWidget.cpp \
	$$SRC/CommitMessageGenerator.cpp \
	$$SRC/CommitPropertyDialog.cpp \
	$$SRC/CommitViewWindow.cpp \
	$$SRC/ConfigCredentialHelperDialog.cpp \
	$$SRC/ConfigSigningDialog.cpp \
	$$SRC/ConfigUserDialog.cpp \
	$$SRC/CreateRepositoryDialog.cpp \
	$$SRC/DeleteBranchDialog.cpp \
	$$SRC/DeleteTagsDialog.cpp \
	$$SRC/DialogHeaderFrame.cpp \
	$$SRC/DoYouWantToInitDialog.cpp \
	$$SRC/DropDownListFrame.cpp \
	$$SRC/EditGitIgnoreDialog.cpp \
	$$SRC/EditProfilesDialog.cpp \
	$$SRC/EditRemoteDialog.cpp \
	$$SRC/EditTagsDialog.cpp \
	$$SRC/ExperimentDialog.cpp \
	$$SRC/FileDiffSliderWidget.cpp \
	$$SRC/FileDiffWidget.cpp \
	$$SRC/FileHistoryWindow.cpp \
	$$SRC/FileListWidget.cpp \
	$$SRC/FilePropertyDialog.cpp \
	$$SRC/FileUtil.cpp \
	$$SRC/FileViewWidget.cpp \
	$$SRC/FindCommitDialog.cpp \
	$$SRC/GenerateCommitMessageDialog.cpp \
	$$SRC/GenerateCommitMessageThread.cpp \
	$$SRC/GeneratedCommitMessage.cpp \
	$$SRC/GenerativeAI.cpp \
	$$SRC/Git.cpp \
	$$SRC/GitBasicSession.cpp \
	$$SRC/GitCommandCache.cpp \
	$$SRC/GitCommandRunner.cpp \
	$$SRC/GitConfigGlobalAddSafeDirectoryDialog.cpp \
	$$SRC/GitDiffManager.cpp \
	$$SRC/GitHubAPI.cpp \
	$$SRC/GitObjectManager.cpp \
	$$SRC/GitPack.cpp \
	$$SRC/GitPackIdxV2.cpp \
	$$SRC/GitProcessThread.cpp \
	$$SRC/GitRunner.cpp \
	$$SRC/GitTypes.cpp \
	$$SRC/HyperLinkLabel.cpp \
	$$SRC/ImageViewWidget.cpp \
	$$SRC/IncrementalSearch.cpp \
	$$SRC/InputNewTagDialog.cpp \
	$$SRC/JumpDialog.cpp \
	$$SRC/Languages.cpp \
	$$SRC/LineEditDialog.cpp \
	$$SRC/LocalSocketReader.cpp \
	$$SRC/Logger.cpp \
	$$SRC/MainWindow.cpp \
	$$SRC/ManageWorkingFolderDialog.cpp \
	$$SRC/MaximizeButton.cpp \
	$$SRC/MemoryReader.cpp \
	$$SRC/MenuButton.cpp \
	$$SRC/MergeDialog.cpp \
	$$SRC/MyImageViewWidget.cpp \
	$$SRC/MyObjectViewBase.cpp \
	$$SRC/MyProcess.cpp \
	$$SRC/MySettings.cpp \
	$$SRC/MyTableWidgetDelegate.cpp \
	$$SRC/MyTextEditorWidget.cpp \
	$$SRC/MyToolButton.cpp \
	$$SRC/OverrideWaitCursor.cpp \
	$$SRC/Photoshop.cpp \
	$$SRC/Profile.cpp \
	$$SRC/ProgressTextLabel.cpp \
	$$SRC/ProgressWidget.cpp \
	$$SRC/PushDialog.cpp \
	$$SRC/ReadOnlyLineEdit.cpp \
	$$SRC/ReadOnlyPlainTextEdit.cpp \
	$$SRC/ReflogWindow.cpp \
	$$SRC/RemoteAdvancedOptionWidget.cpp \
	$$SRC/RemoteRepositoriesTableWidget.cpp \
	$$SRC/RepositoryInfo.cpp \
	$$SRC/RepositoryInfoFrame.cpp \
	$$SRC/RepositoryModel.cpp \
	$$SRC/RepositoryPropertyDialog.cpp \
	$$SRC/RepositoryTreeWidget.cpp \
	$$SRC/RepositoryUrlLineEdit.cpp \
	$$SRC/SearchFromGitHubDialog.cpp \
	$$SRC/SelectCommandDialog.cpp \
	$$SRC/SelectGpgKeyDialog.cpp \
	$$SRC/SelectItemDialog.cpp \
	$$SRC/SetGlobalUserDialog.cpp \
	$$SRC/SetGpgSigningDialog.cpp \
	$$SRC/SettingAiForm.cpp \
	$$SRC/SettingBehaviorForm.cpp \
	$$SRC/SettingExampleForm.cpp \
	$$SRC/SettingGeneralForm.cpp \
	$$SRC/SettingLoggingForm.cpp \
	$$SRC/SettingNetworkForm.cpp \
	$$SRC/SettingOptionsForm.cpp \
	$$SRC/SettingPrograms2Form.cpp \
	$$SRC/SettingProgramsForm.cpp \
	$$SRC/SettingVisualForm.cpp \
	$$SRC/SettingWorkingFolderForm.cpp \
	$$SRC/SettingsDialog.cpp \
	$$SRC/SimpleImageWidget.cpp \
	$$SRC/SimpleQtIO.cpp \
	$$SRC/StatusInfo.cpp \
	$$SRC/StatusLabel.cpp \
	$$SRC/SubmoduleAddDialog.cpp \
	$$SRC/SubmoduleUpdateDialog.cpp \
	$$SRC/SubmodulesDialog.cpp \
	$$SRC/Terminal.cpp \
	$$SRC/TextEditDialog.cpp \
	$$SRC/Theme.cpp \
	$$SRC/TraceEventWriter.cpp \
	$$SRC/TraceLogger.cpp \
	$$SRC/UserEvent.cpp \
	$$SRC/Util.cpp \
	$$SRC/WelcomeWizardDialog.cpp \
	$$SRC/WorkingDirLineEdit.cpp \
	$$SRC/XmlTagState.cpp \
	$$SRC/coloredit/ColorDialog.cpp \
	$$SRC/coloredit/ColorEditWidget.cpp \
	$$SRC/coloredit/ColorPreviewWidget.cpp \
	$$SRC/coloredit/ColorSlider.cpp \
	$$SRC/coloredit/ColorSquareWidget.cpp \
	$$SRC/coloredit/RingSlider.cpp \
	$$SRC/common/AbstractSimpleIO.cpp \
	$$SRC/common/misc.cpp \
	$$SRC/common/q/Dir.cpp \
	$$SRC/common/q/FileInfo.cpp \
	$$SRC/common/urlencode.cpp \
	$$SRC/darktheme/DarkStyle.cpp \
	$$SRC/darktheme/LightStyle.cpp \
	$$SRC/darktheme/NinePatch.cpp \
	$$SRC/darktheme/TraditionalWindowsStyleTreeControl.cpp \
	$$SRC/gpg.cpp \
	$$SRC/gzip.cpp \
	$$SRC/inetclient.cpp \
	$$SRC/inetresolver.cpp \
	$$SRC/main.cpp\
	$$SRC/platform.cpp \
	$$SRC/texteditor/AbstractCharacterBasedApplication.cpp \
	$$SRC/texteditor/InputMethodPopup.cpp \
	$$SRC/texteditor/TextEditorTheme.cpp \
	$$SRC/texteditor/TextEditorView.cpp \
	$$SRC/texteditor/TextEditorWidget.cpp \
	$$SRC/texteditor/UnicodeWidth.cpp \
	$$SRC/texteditor/unicode.cpp \
	$$SRC/webclient.cpp \
	$$SRC/zip/zip.cpp \
	$$SRC/zip/ziparchive.cpp \
	$$SRC/zip/zipextract.cpp

HEADERS += \
	$$SRC/FileTypeDetector.h \
	$$SRC/process/ProcessExample.h \
	$$SRC/common/fmt.h \
	$$SRC/common/q/DateTime.h \
	$$SRC/common/q/DirIterator.h \
	$$SRC/common/qmisc.h \
	$$SRC/common/str.h \
	$$SRC/process/MyProcess2.h \
	$$SRC/process/ProcessPosix.h \
	$$SRC/AboutDialog.h \
	$$SRC/AbstractGitSession.h \
	$$SRC/AbstractProcess.h \
	$$SRC/AbstractSettingForm.h \
	$$SRC/AddRepositoriesCollectivelyDialog.h \
	$$SRC/AddRepositoryDialog.h \
	$$SRC/ApplicationGlobal.h \
	$$SRC/ApplicationSettings.h \
	$$SRC/AreYouSureYouWantToContinueConnectingDialog.h \
	$$SRC/AvatarLoader.h \
	$$SRC/BigDiffWindow.h \
	$$SRC/BlameWindow.h \
	$$SRC/BlockSignals.h \
	$$SRC/BranchLabel.h \
	$$SRC/CheckoutDialog.h \
	$$SRC/CherryPickDialog.h \
	$$SRC/CleanSubModuleDialog.h \
	$$SRC/ClearButton.h \
	$$SRC/CloneFromGitHubDialog.h \
	$$SRC/ColorButton.h \
	$$SRC/CommitDetailGetter.h \
	$$SRC/CommitDialog.h \
	$$SRC/CommitExploreWindow.h \
	$$SRC/CommitLogTableWidget.h \
	$$SRC/CommitMessageGenerator.h \
	$$SRC/CommitPropertyDialog.h \
	$$SRC/CommitViewWindow.h \
	$$SRC/ConfigCredentialHelperDialog.h \
	$$SRC/ConfigSigningDialog.h \
	$$SRC/ConfigUserDialog.h \
	$$SRC/CreateRepositoryDialog.h \
	$$SRC/DeleteBranchDialog.h \
	$$SRC/DeleteTagsDialog.h \
	$$SRC/DialogHeaderFrame.h \
	$$SRC/DoYouWantToInitDialog.h \
	$$SRC/DropDownListFrame.h \
	$$SRC/EditGitIgnoreDialog.h \
	$$SRC/EditProfilesDialog.h \
	$$SRC/EditRemoteDialog.h \
	$$SRC/EditTagsDialog.h \
	$$SRC/ExperimentDialog.h \
	$$SRC/FileDiffSliderWidget.h \
	$$SRC/FileDiffWidget.h \
	$$SRC/FileHistoryWindow.h \
	$$SRC/FileListWidget.h \
	$$SRC/FilePropertyDialog.h \
	$$SRC/FileUtil.h \
	$$SRC/FindCommitDialog.h \
	$$SRC/GenerateCommitMessageDialog.h \
	$$SRC/GenerateCommitMessageThread.h \
	$$SRC/GeneratedCommitMessage.h \
	$$SRC/GenerativeAI.h \
	$$SRC/GenerativeAI.h \
	$$SRC/Git.h \
	$$SRC/GitBasicSession.h \
	$$SRC/GitCommandCache.h \
	$$SRC/GitCommandRunner.h \
	$$SRC/GitConfigGlobalAddSafeDirectoryDialog.h \
	$$SRC/GitDiffManager.h \
	$$SRC/GitHubAPI.h \
	$$SRC/GitObjectManager.h \
	$$SRC/GitPack.h \
	$$SRC/GitPackIdxV2.h \
	$$SRC/GitProcessThread.h \
	$$SRC/GitRunner.h \
	$$SRC/GitTypes.h \
	$$SRC/HyperLinkLabel.h \
	$$SRC/ImageViewWidget.h \
	$$SRC/IncrementalSearch.h \
	$$SRC/InputNewTagDialog.h \
	$$SRC/JumpDialog.h \
	$$SRC/Languages.h \
	$$SRC/LineEditDialog.h \
	$$SRC/LocalSocketReader.h \
	$$SRC/Logger.h \
	$$SRC/MainWindow.h \
	$$SRC/ManageWorkingFolderDialog.h \
	$$SRC/MaximizeButton.h \
	$$SRC/MemoryReader.h \
	$$SRC/MenuButton.h \
	$$SRC/MergeDialog.h \
	$$SRC/MyImageViewWidget.h \
	$$SRC/MyObjectViewBase.h \
	$$SRC/MyProcess.h \
	$$SRC/MySettings.h \
	$$SRC/MyTableWidgetDelegate.h \
	$$SRC/MyTextEditorWidget.h \
	$$SRC/MyToolButton.h \
	$$SRC/OverrideWaitCursor.h \
	$$SRC/Photoshop.h \
	$$SRC/Profile.h \
	$$SRC/ProgressTextLabel.h \
	$$SRC/ProgressWidget.h \
	$$SRC/PushDialog.h \
	$$SRC/ReadOnlyLineEdit.h \
	$$SRC/ReadOnlyPlainTextEdit.h \
	$$SRC/ReflogWindow.h \
	$$SRC/RemoteAdvancedOptionWidget.h \
	$$SRC/RemoteRepositoriesTableWidget.h \
	$$SRC/RepositoryInfo.h \
	$$SRC/RepositoryInfoFrame.h \
	$$SRC/RepositoryModel.h \
	$$SRC/RepositoryPropertyDialog.h \
	$$SRC/RepositorySearchResultItem.h \
	$$SRC/RepositoryTreeWidget.h \
	$$SRC/RepositoryUrlLineEdit.h \
	$$SRC/SearchFromGitHubDialog.h \
	$$SRC/SelectCommandDialog.h \
	$$SRC/SelectGpgKeyDialog.h \
	$$SRC/SelectItemDialog.h \
	$$SRC/SetGlobalUserDialog.h \
	$$SRC/SetGpgSigningDialog.h \
	$$SRC/SettingAiForm.h \
	$$SRC/SettingBehaviorForm.h \
	$$SRC/SettingExampleForm.h \
	$$SRC/SettingGeneralForm.h \
	$$SRC/SettingLoggingForm.h \
	$$SRC/SettingNetworkForm.h \
	$$SRC/SettingOptionsForm.h \
	$$SRC/SettingPrograms2Form.h \
	$$SRC/SettingProgramsForm.h \
	$$SRC/SettingVisualForm.h \
	$$SRC/SettingWorkingFolderForm.h \
	$$SRC/SettingsDialog.h \
	$$SRC/SimpleImageWidget.h \
	$$SRC/SimpleQtIO.h \
	$$SRC/StatusInfo.h \
	$$SRC/StatusLabel.h \
	$$SRC/SubmoduleAddDialog.h \
	$$SRC/SubmoduleUpdateDialog.h \
	$$SRC/SubmodulesDialog.h \
	$$SRC/Terminal.h \
	$$SRC/TextEditDialog.h \
	$$SRC/Theme.h \
	$$SRC/TraceEventWriter.h \
	$$SRC/TraceLogger.h \
	$$SRC/UserEvent.h \
	$$SRC/Util.h \
	$$SRC/WelcomeWizardDialog.h \
	$$SRC/WorkingDirLineEdit.h \
	$$SRC/XmlTagState.h \
	$$SRC/coloredit/ColorDialog.h \
	$$SRC/coloredit/ColorEditWidget.h \
	$$SRC/coloredit/ColorPreviewWidget.h \
	$$SRC/coloredit/ColorSlider.h \
	$$SRC/coloredit/ColorSquareWidget.h \
	$$SRC/coloredit/RingSlider.h \
	$$SRC/common/AbstractSimpleIO.h \
	$$SRC/common/base64.h \
	$$SRC/common/charvec.h \
	$$SRC/common/crc32.h \
	$$SRC/common/htmlencode.h \
	$$SRC/common/joinpath.h \
	$$SRC/common/jstream.h \
	$$SRC/common/misc.h \
	$$SRC/common/q/Dir.h \
	$$SRC/common/q/FileInfo.h \
	$$SRC/common/q/helper.h \
	$$SRC/common/strformat.h \
	$$SRC/common/urlencode.h \
	$$SRC/darktheme/DarkStyle.h \
	$$SRC/darktheme/LightStyle.h \
	$$SRC/darktheme/NinePatch.h \
	$$SRC/darktheme/TraditionalWindowsStyleTreeControl.h \
	$$SRC/dtl/Diff.hpp \
	$$SRC/dtl/Diff3.hpp \
	$$SRC/dtl/Lcs.hpp \
	$$SRC/dtl/Sequence.hpp \
	$$SRC/dtl/Ses.hpp \
	$$SRC/dtl/dtl.hpp \
	$$SRC/dtl/functors.hpp \
	$$SRC/dtl/variables.hpp \
	$$SRC/gpg.h \
	$$SRC/gzip.h \
	$$SRC/inetclient.h \
	$$SRC/inetresolver.h \
	$$SRC/platform.h \
	$$SRC/texteditor/AbstractCharacterBasedApplication.h \
	$$SRC/texteditor/InputMethodPopup.h \
	$$SRC/texteditor/TextEditorTheme.h \
	$$SRC/texteditor/TextEditorView.h \
	$$SRC/texteditor/TextEditorWidget.h \
	$$SRC/texteditor/UnicodeWidth.h \
	$$SRC/texteditor/unicode.h \
	$$SRC/webclient.h \
	$$SRC/zip/zip.h \
	$$SRC/zip/zipinternal.h

HEADERS += version.h

FORMS += \
	$$SRC/SettingLoggingForm.ui \
	$$SRC/AboutDialog.ui \
	$$SRC/AddRepositoriesCollectivelyDialog.ui \
	$$SRC/AddRepositoryDialog.ui \
	$$SRC/AreYouSureYouWantToContinueConnectingDialog.ui \
	$$SRC/BigDiffWindow.ui \
	$$SRC/BlameWindow.ui \
	$$SRC/CheckoutDialog.ui \
	$$SRC/CherryPickDialog.ui \
	$$SRC/CleanSubModuleDialog.ui \
	$$SRC/CloneFromGitHubDialog.ui \
	$$SRC/CommitDialog.ui \
	$$SRC/CommitExploreWindow.ui \
	$$SRC/CommitPropertyDialog.ui \
	$$SRC/CommitViewWindow.ui \
	$$SRC/ConfigCredentialHelperDialog.ui \
	$$SRC/ConfigSigningDialog.ui \
	$$SRC/ConfigUserDialog.ui \
	$$SRC/CreateRepositoryDialog.ui \
	$$SRC/DeleteBranchDialog.ui \
	$$SRC/DeleteTagsDialog.ui \
	$$SRC/DoYouWantToInitDialog.ui \
	$$SRC/EditGitIgnoreDialog.ui \
	$$SRC/EditProfilesDialog.ui \
	$$SRC/EditRemoteDialog.ui \
	$$SRC/EditTagsDialog.ui \
	$$SRC/ExperimentDialog.ui \
	$$SRC/FileDiffWidget.ui \
	$$SRC/FileHistoryWindow.ui \
	$$SRC/FilePropertyDialog.ui \
	$$SRC/FindCommitDialog.ui \
	$$SRC/GenerateCommitMessageDialog.ui \
	$$SRC/GitConfigGlobalAddSafeDirectoryDialog.ui \
	$$SRC/InputNewTagDialog.ui \
	$$SRC/JumpDialog.ui \
	$$SRC/LineEditDialog.ui \
	$$SRC/MainWindow.ui \
	$$SRC/ManageWorkingFolderDialog.ui \
	$$SRC/MergeDialog.ui \
	$$SRC/ProgressWidget.ui \
	$$SRC/PushDialog.ui \
	$$SRC/ReflogWindow.ui \
	$$SRC/RemoteAdvancedOptionWidget.ui \
	$$SRC/RepositoryPropertyDialog.ui \
	$$SRC/SearchFromGitHubDialog.ui \
	$$SRC/SelectCommandDialog.ui \
	$$SRC/SelectGpgKeyDialog.ui \
	$$SRC/SelectItemDialog.ui \
	$$SRC/SetGlobalUserDialog.ui \
	$$SRC/SetGpgSigningDialog.ui \
	$$SRC/SettingAiForm.ui \
	$$SRC/SettingBehaviorForm.ui \
	$$SRC/SettingExampleForm.ui \
	$$SRC/SettingGeneralForm.ui \
	$$SRC/SettingNetworkForm.ui \
	$$SRC/SettingOptionsForm.ui \
	$$SRC/SettingPrograms2Form.ui \
	$$SRC/SettingProgramsForm.ui \
	$$SRC/SettingVisualForm.ui \
	$$SRC/SettingWorkingFolderForm.ui \
	$$SRC/SettingsDialog.ui \
	$$SRC/SubmoduleAddDialog.ui \
	$$SRC/SubmoduleUpdateDialog.ui \
	$$SRC/SubmodulesDialog.ui \
	$$SRC/TextEditDialog.ui \
	$$SRC/WelcomeWizardDialog.ui \
	$$SRC/coloredit/ColorDialog.ui \
	$$SRC/coloredit/ColorEditWidget.ui

RESOURCES += \
	$$SRC/resources/resources.qrc

unix {
	SOURCES += \
		$$SRC/process/MyProcess2.cpp \
		$$SRC/unix/UnixUtil.cpp \
		$$SRC/unix/UnixProcess.cpp \
		$$SRC/unix/UnixPtyProcess.cpp
	HEADERS += \
		$$SRC/process/ProcessPosix.cpp \
		$$SRC/unix/UnixUtil.h \
		$$SRC/unix/UnixProcess.h \
		$$SRC/unix/UnixPtyProcess.h
}

win32 {
	SOURCES += \
	$$SRC/win32/Win32Util.cpp \
		$$SRC/win32/Win32Process.cpp \
		$$SRC/win32/Win32PtyProcess.cpp \
		$$SRC/win32/event.cpp \
		$$SRC/win32/thread.cpp

	HEADERS += \
	$$SRC/win32/Win32Util.h \
		$$SRC/win32/Win32Process.h \
		$$SRC/win32/Win32PtyProcess.h \
		$$SRC/win32/event.h \
		$$SRC/win32/mutex.h \
		$$SRC/win32/thread.h

	LIBS += -lole32
}

unsafe {
	SOURCES += \
		$$SRC/sshsupport/ConfirmRemoteSessionDialog.cpp \
		$$SRC/sshsupport/GitRemoteSshSession.cpp \
		$$SRC/sshsupport/Quissh.cpp \
		$$SRC/sshsupport/SshDialog.cpp
	HEADERS += \
		$$SRC/sshsupport/ConfirmRemoteSessionDialog.h \
		$$SRC/sshsupport/GitRemoteSshSession.h \
		$$SRC/sshsupport/Quissh.h \
		$$SRC/sshsupport/SshDialog.h
	FORMS += \
		$$SRC/sshsupport/ConfirmRemoteSessionDialog.ui \
		$$SRC/sshsupport/SshDialog.ui
}

use_libcurl {
	SOURCES += $$SRC/curlclient.cpp
	HEADERS += $$SRC/curlclient.h
}

