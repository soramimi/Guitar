
QT += core gui widgets svg network

win32:lessThan(QT_MAJOR_VERSION, 6) {
	QT += winextras
}

TARGET = Guitar
TEMPLATE = app

# CONFIG += unsafe ### don't enable
unsafe {
	DEFINES += UNSAFE_ENABLED
	LIBS += -lssh
}

win32:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

SRC = $$PWD/src/

TRANSLATIONS = $$SRC/resources/translations/Guitar_ja.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_ru.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_es.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_ta.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_zh-CN.ts
TRANSLATIONS += $$SRC/resources/translations/Guitar_zh-TW.ts

DEFINES += USE_QT
DEFINES += APP_GUITAR

DEFINES += HAVE_POSIX_OPENPT
macx:DEFINES += HAVE_SYS_TIME_H
macx:DEFINES += HAVE_UTMPX

linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += $$PROCESS_SRCexecutable_path/../Frameworks

INCLUDEPATH += $$SRC
INCLUDEPATH += $$SRC/common
INCLUDEPATH += $$SRC/coloredit
INCLUDEPATH += $$SRC/texteditor
# INCLUDEPATH += $$PWD/subprojects/FileTypePlugin/src
# INCLUDEPATH += $$PWD/subprojects/IncrementalSearchPlugin/src
# INCLUDEPATH += $$PWD/subprojects/OnePasswordPlugin/src

win32:INCLUDEPATH += $$PWD/misc/winpty/include
win32:LIBS += $$PWD/misc/winpty/x64/lib/winpty.lib -lshlwapi

# execute 'ruby prepare.rb' automatically

prepare.target = prepare
prepare.commands = cd $$PWD && ruby -W0 prepare.rb
QMAKE_EXTRA_TARGETS += prepare
PRE_TARGETDEPS += prepare

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
win32:LIBS += -llibcrypto -llibssl

# network library

# CONFIG += use_libcurl
use_libcurl {
	DEFINES += USE_LIBCURL
	!win32:LIBS += -lcurl
	win32:LIBS += -llibcurl
}

# experimental: Sudachi support

# CONFIG += use_cudachi
use_cudachi {
	DEFINES += USE_SUDACHI
	CONFIG(debug,debug|release):IBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/release -lsudachi_capi
	CONFIG(release,debug|release):IBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/debug -lsudachi_capi
}

# zlib

win32 {
	LIBS += -lzlib
	LIBS += -lzstd
}

!win32 {
	LIBS += -lz
	LIBS += -lzstd
}

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

!win32 {
	LIBS += -ldl
}

#

SOURCES += \
	$$PWD/src/MyProcess.cpp \
	$$PWD/subprojects/FileTypePlugin/src/FileTypeWrapper.cpp \
	$$SRC/CommitRecord.cpp \
	$$SRC/IncrementalSearchHelper.cpp \
	$$SRC/LoadPlugin.cpp \
	$$SRC/AboutDialog.cpp \
	$$SRC/AbstractGitSession.cpp \
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
	$$SRC/FileTypeDetector.cpp \
	$$SRC/FileUtil.cpp \
	$$SRC/FileViewWidget.cpp \
	$$SRC/FindCommitDialog.cpp \
	$$SRC/GenerateCommitMessageDialog.cpp \
	$$SRC/GenerateCommitMessageThread.cpp \
	$$SRC/GeneratedCommitMessage.cpp \
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
	$$SRC/ai/AiApiBridge.cpp \
	$$SRC/ai/CommitMessageGenerator.cpp \
	$$SRC/ai/GenerativeAI.cpp \
	$$SRC/coloredit/ColorDialog.cpp \
	$$SRC/coloredit/ColorEditWidget.cpp \
	$$SRC/coloredit/ColorPreviewWidget.cpp \
	$$SRC/coloredit/ColorSlider.cpp \
	$$SRC/coloredit/ColorSquareWidget.cpp \
	$$SRC/coloredit/RingSlider.cpp \
	$$SRC/common/AbstractSimpleIO.cpp \
	$$SRC/common/misc.cpp \
	$$SRC/common/npos.cpp \
	$$SRC/common/q/DateTime.cpp \
	$$SRC/common/q/Dir.cpp \
	$$SRC/common/q/DirIterator.cpp \
	$$SRC/common/q/FileInfo.cpp \
	$$SRC/common/qmisc.cpp \
	$$SRC/common/realpath.cpp \
	$$SRC/common/unicode_conversion.cpp \
	$$SRC/common/urlencode.cpp \
	$$SRC/darktheme/DarkStyle.cpp \
	$$SRC/darktheme/LightStyle.cpp \
	$$SRC/darktheme/MyCommonStyle.cpp \
	$$SRC/darktheme/NinePatch.cpp \
	$$SRC/darktheme/TraditionalWindowsStyleTreeControl.cpp \
	$$SRC/gpg.cpp \
	$$SRC/gzip.cpp \
	$$SRC/inet/inetclient.cpp \
	$$SRC/inet/inetresolver.cpp \
	$$SRC/inet/webclient.cpp \
	$$SRC/main.cpp\
	$$SRC/platform.cpp \
	$$SRC/texteditor/AbstractCharacterBasedApplication.cpp \
	$$SRC/texteditor/InputMethodPopup.cpp \
	$$SRC/texteditor/TextEditorTheme.cpp \
	$$SRC/texteditor/TextEditorView.cpp \
	$$SRC/texteditor/TextEditorWidget.cpp \
	$$SRC/texteditor/UnicodeWidth.cpp \
	$$SRC/texteditor/unicode.cpp \
	$$SRC/zip/zip.cpp \
	$$SRC/zip/ziparchive.cpp \
	$$SRC/zip/zipextract.cpp

HEADERS += \
	$$PWD/subprojects/FileTypePlugin/src/FileTypeWrapper.h \
	$$SRC/CommitRecord.h \
	$$SRC/IncrementalSearchHelper.h \
	$$SRC/LoadPlugin.h \
	$$SRC/AboutDialog.h \
	$$SRC/AbstractGitSession.h \
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
	$$SRC/FileTypeDetector.h \
	$$SRC/FileUtil.h \
	$$SRC/FindCommitDialog.h \
	$$SRC/GenerateCommitMessageDialog.h \
	$$SRC/GenerateCommitMessageThread.h \
	$$SRC/GeneratedCommitMessage.h \
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
	$$SRC/ai/AiApiBridge.h \
	$$SRC/ai/CommitMessageGenerator.h \
	$$SRC/ai/GenerativeAI.h \
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
	$$SRC/common/fmt.h \
	$$SRC/common/htmlencode.h \
	$$SRC/common/joinpath.h \
	$$SRC/common/jstream.h \
	$$SRC/common/misc.h \
	$$SRC/common/npos.h \
	$$SRC/common/q/DateTime.h \
	$$SRC/common/q/Dir.h \
	$$SRC/common/q/DirIterator.h \
	$$SRC/common/q/FileInfo.h \
	$$SRC/common/q/helper.h \
	$$SRC/common/qmisc.h \
	$$SRC/common/realpath.h \
	$$SRC/common/str.h \
	$$SRC/common/strformat.h \
	$$SRC/common/unicode_conversion.h \
	$$SRC/common/urlencode.h \
	$$SRC/darktheme/DarkStyle.h \
	$$SRC/darktheme/LightStyle.h \
	$$SRC/darktheme/MyCommonStyle.h \
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
	$$SRC/inet/inetclient.h \
	$$SRC/inet/inetresolver.h \
	$$SRC/inet/webclient.h \
	$$SRC/platform.h \
	$$SRC/texteditor/AbstractCharacterBasedApplication.h \
	$$SRC/texteditor/InputMethodPopup.h \
	$$SRC/texteditor/TextEditorTheme.h \
	$$SRC/texteditor/TextEditorView.h \
	$$SRC/texteditor/TextEditorWidget.h \
	$$SRC/texteditor/UnicodeWidth.h \
	$$SRC/texteditor/unicode.h \
	$$SRC/zip/zip.h \
	$$SRC/zip/zipinternal.h

FORMS += \
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
	$$SRC/SettingLoggingForm.ui \
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
		$$SRC/unix/UnixUtil.cpp
	HEADERS += \
		$$SRC/unix/UnixUtil.h
}

win32 {
	SOURCES += $$PWD/src/SettingWindowsForm.cpp
	HEADERS += $$PWD/src/SettingWindowsForm.h
	FORMS += $$PWD/src/SettingWindowsForm.ui
	SOURCES += $$SRC/win32/Win32Util.cpp $$SRC/common/wstring.cpp
	HEADERS += $$SRC/win32/Win32Util.h $$SRC/common/wstring.h
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
	SOURCES += $$SRC/inet/curlclient.cpp
	HEADERS += $$SRC/inet/curlclient.h
}

PROCESS_SRC += $$SRC/process/src
PROCESS_PRI = $$PROCESS_SRC/../process.pri
INCLUDEPATH += $$PROCESS_SRC
DISTFILES += $$PROCESS_PRI
include($$PROCESS_PRI)

