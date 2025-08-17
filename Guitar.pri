
QMAKE_PROJECT_DEPTH = 0

QT += core gui widgets svg network

msvc:lessThan(QT_MAJOR_VERSION, 6) {
    QT += winextras
}

TARGET = Guitar
TEMPLATE = app

CPP_STD = c++17

CONFIG += $$CPP_STD nostrip debug_info static

# CONFIG += unsafe
unsafe {
    DEFINES += UNSAFE_ENABLED
	msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib
	LIBS += -lssh
}

msvc:DEFINES += NOMINMAX

INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

TRANSLATIONS = $$PWD/src/resources/translations/Guitar_ja.ts
TRANSLATIONS += $$PWD/src/resources/translations/Guitar_ru.ts
TRANSLATIONS += $$PWD/src/resources/translations/Guitar_es.ts
TRANSLATIONS += $$PWD/src/resources/translations/Guitar_zh-CN.ts
TRANSLATIONS += $$PWD/src/resources/translations/Guitar_zh-TW.ts

DEFINES += APP_GUITAR

DEFINES += HAVE_POSIX_OPENPT
macx:DEFINES += HAVE_SYS_TIME_H
macx:DEFINES += HAVE_UTMPX

gcc:QMAKE_CXXFLAGS += -std=$$CPP_STD -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter
linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += @executable_path/../Frameworks

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/src/coloredit
INCLUDEPATH += $$PWD/src/texteditor
# INCLUDEPATH += $$PWD/filetype/src

msvc:INCLUDEPATH += $$PWD/misc/winpty/include
msvc:LIBS += $$PWD/misc/winpty/x64/lib/winpty.lib -lshlwapi
# msvc:QMAKE_CFLAGS += /FI $$PWD/filetype/misc/win32/unistd.h

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
# !msvc:LIBS += $$PWD/filetype/lib/libfiletyped.a $$PWD/filetype/lib/libonigurumad.a $$PWD/filetype/lib/libfiled.a

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
	$$PWD/src/GitDiffManager.cpp \
	$$PWD/src/GitRunner.cpp \
	$$PWD/src/GitTypes.cpp \
    $$PWD/src/SettingLoggingForm.cpp \
	$$PWD/src/SimpleQtIO.cpp \
    $$PWD/src/TraceEventWriter.cpp \
    $$PWD/src/TraceLogger.cpp \
	$$PWD/src/common/AbstractSimpleIO.cpp \
    $$PWD/src/common/crc32.cpp \
	$$PWD/src/common/htmlencode.cpp \
	$$PWD/src/gzip.cpp \
	$$PWD/udplogger/RemoteLogger.cpp \
    src/AboutDialog.cpp \
	src/AbstractGitSession.cpp \
	src/AbstractProcess.cpp \
	src/AbstractSettingForm.cpp \
	src/AddRepositoriesCollectivelyDialog.cpp \
	src/AddRepositoryDialog.cpp \
	src/ApplicationGlobal.cpp \
	src/ApplicationSettings.cpp \
	src/AreYouSureYouWantToContinueConnectingDialog.cpp \
	src/AvatarLoader.cpp \
	src/BigDiffWindow.cpp \
	src/BlameWindow.cpp \
	src/BlockSignals.cpp \
	src/BranchLabel.cpp \
	src/CheckoutDialog.cpp \
	src/CherryPickDialog.cpp \
	src/CleanSubModuleDialog.cpp \
	src/ClearButton.cpp \
	src/CloneFromGitHubDialog.cpp \
	src/ColorButton.cpp \
	src/CommitDetailGetter.cpp \
	src/CommitDialog.cpp \
	src/CommitExploreWindow.cpp \
	src/CommitLogTableWidget.cpp \
	src/CommitMessageGenerator.cpp \
	src/CommitPropertyDialog.cpp \
	src/CommitViewWindow.cpp \
	src/ConfigCredentialHelperDialog.cpp \
	src/ConfigSigningDialog.cpp \
	src/ConfigUserDialog.cpp \
	src/CreateRepositoryDialog.cpp \
	src/DeleteBranchDialog.cpp \
	src/DeleteTagsDialog.cpp \
	src/DialogHeaderFrame.cpp \
	src/DoYouWantToInitDialog.cpp \
	src/DropDownListFrame.cpp \
	src/EditGitIgnoreDialog.cpp \
	src/EditProfilesDialog.cpp \
	src/EditRemoteDialog.cpp \
	src/EditTagsDialog.cpp \
	src/ExperimentDialog.cpp \
	src/FileDiffSliderWidget.cpp \
	src/FileDiffWidget.cpp \
	src/FileHistoryWindow.cpp \
	src/FileListWidget.cpp \
	src/FilePropertyDialog.cpp \
	src/FileUtil.cpp \
	src/FileViewWidget.cpp \
	src/FindCommitDialog.cpp \
	src/GenerateCommitMessageDialog.cpp \
	src/GenerateCommitMessageThread.cpp \
	src/GeneratedCommitMessage.cpp \
	src/GenerativeAI.cpp \
	src/Git.cpp \
	src/GitBasicSession.cpp \
	src/GitCommandCache.cpp \
	src/GitCommandRunner.cpp \
	src/GitConfigGlobalAddSafeDirectoryDialog.cpp \
	src/GitHubAPI.cpp \
	src/GitObjectManager.cpp \
	src/GitPack.cpp \
	src/GitPackIdxV2.cpp \
	src/GitProcessThread.cpp \
	src/HyperLinkLabel.cpp \
	src/ImageViewWidget.cpp \
	src/IncrementalSearch.cpp \
	src/InputNewTagDialog.cpp \
	src/JumpDialog.cpp \
	src/Languages.cpp \
	src/LineEditDialog.cpp \
	src/LocalSocketReader.cpp \
	src/MainWindow.cpp \
	src/ManageWorkingFolderDialog.cpp \
	src/MaximizeButton.cpp \
	src/MemoryReader.cpp \
	src/MenuButton.cpp \
	src/MergeDialog.cpp \
	src/MyImageViewWidget.cpp \
	src/MyProcess.cpp \
	src/MySettings.cpp \
	src/MyTableWidgetDelegate.cpp \
	src/MyTextEditorWidget.cpp \
	src/MyToolButton.cpp \
	src/OverrideWaitCursor.cpp \
	src/Photoshop.cpp \
	src/Profile.cpp \
	src/ProgressTextLabel.cpp \
	src/ProgressWidget.cpp \
	src/PushDialog.cpp \
	src/ReadOnlyLineEdit.cpp \
	src/ReadOnlyPlainTextEdit.cpp \
	src/ReflogWindow.cpp \
	src/RemoteAdvancedOptionWidget.cpp \
	src/RemoteRepositoriesTableWidget.cpp \
	src/RepositoryInfo.cpp \
	src/RepositoryInfoFrame.cpp \
	src/RepositoryModel.cpp \
	src/RepositoryPropertyDialog.cpp \
	src/RepositoryTreeWidget.cpp \
	src/RepositoryUrlLineEdit.cpp \
	src/SearchFromGitHubDialog.cpp \
	src/SelectCommandDialog.cpp \
	src/SelectGpgKeyDialog.cpp \
	src/SelectItemDialog.cpp \
	src/SetGlobalUserDialog.cpp \
	src/SetGpgSigningDialog.cpp \
	src/SettingAiForm.cpp \
	src/SettingBehaviorForm.cpp \
	src/SettingExampleForm.cpp \
	src/SettingGeneralForm.cpp \
	src/SettingNetworkForm.cpp \
	src/SettingOptionsForm.cpp \
	src/SettingPrograms2Form.cpp \
	src/SettingProgramsForm.cpp \
	src/SettingVisualForm.cpp \
	src/SettingWorkingFolderForm.cpp \
	src/SettingsDialog.cpp \
	src/SimpleImageWidget.cpp \
	src/StatusInfo.cpp \
	src/StatusLabel.cpp \
	src/SubmoduleAddDialog.cpp \
	src/SubmoduleUpdateDialog.cpp \
	src/SubmodulesDialog.cpp \
	src/Terminal.cpp \
	src/TextEditDialog.cpp \
	src/Theme.cpp \
	src/UserEvent.cpp \
	src/Util.cpp \
	src/WelcomeWizardDialog.cpp \
	src/WorkingDirLineEdit.cpp \
	src/XmlTagState.cpp \
	src/coloredit/ColorDialog.cpp \
	src/coloredit/ColorEditWidget.cpp \
	src/coloredit/ColorPreviewWidget.cpp \
	src/coloredit/ColorSlider.cpp \
	src/coloredit/ColorSquareWidget.cpp \
	src/coloredit/RingSlider.cpp \
	src/common/base64.cpp \
	src/common/charvec.cpp \
	src/common/joinpath.cpp \
	src/common/misc.cpp \
	src/darktheme/DarkStyle.cpp \
	src/darktheme/LightStyle.cpp \
	src/darktheme/NinePatch.cpp \
	src/darktheme/TraditionalWindowsStyleTreeControl.cpp \
	src/gpg.cpp \
	src/main.cpp\
	src/platform.cpp \
	src/texteditor/AbstractCharacterBasedApplication.cpp \
	src/texteditor/InputMethodPopup.cpp \
	src/texteditor/TextEditorTheme.cpp \
	src/texteditor/TextEditorView.cpp \
	src/texteditor/TextEditorWidget.cpp \
	src/texteditor/UnicodeWidth.cpp \
	src/texteditor/unicode.cpp \
	src/urlencode.cpp \
	src/webclient.cpp \
	src/zip/zip.cpp \
	src/zip/ziparchive.cpp \
	src/zip/zipextract.cpp

HEADERS += \
	$$PWD/src/GitDiffManager.h \
	$$PWD/src/GitRunner.h \
    $$PWD/src/SettingLoggingForm.h \
	$$PWD/src/SimpleQtIO.h \
    $$PWD/src/TraceEventWriter.h \
    $$PWD/src/TraceLogger.h \
	$$PWD/src/common/AbstractSimpleIO.h \
    $$PWD/src/common/crc32.h \
	$$PWD/src/common/htmlencode.h \
	$$PWD/src/gzip.h \
	$$PWD/udplogger/RemoteLogger.h \
    src/AboutDialog.h \
	src/AbstractGitSession.h \
	src/AbstractProcess.h \
	src/AbstractSettingForm.h \
	src/AddRepositoriesCollectivelyDialog.h \
	src/AddRepositoryDialog.h \
	src/ApplicationGlobal.h \
	src/ApplicationSettings.h \
	src/AreYouSureYouWantToContinueConnectingDialog.h \
	src/AvatarLoader.h \
	src/BigDiffWindow.h \
	src/BlameWindow.h \
	src/BlockSignals.h \
	src/BranchLabel.h \
	src/CheckoutDialog.h \
	src/CherryPickDialog.h \
	src/CleanSubModuleDialog.h \
	src/ClearButton.h \
	src/CloneFromGitHubDialog.h \
	src/ColorButton.h \
	src/CommitDetailGetter.h \
	src/CommitDialog.h \
	src/CommitExploreWindow.h \
	src/CommitLogTableWidget.h \
	src/CommitMessageGenerator.h \
	src/CommitPropertyDialog.h \
	src/CommitViewWindow.h \
	src/ConfigCredentialHelperDialog.h \
	src/ConfigSigningDialog.h \
	src/ConfigUserDialog.h \
	src/CreateRepositoryDialog.h \
	src/Debug.h \
	src/DeleteBranchDialog.h \
	src/DeleteTagsDialog.h \
	src/DialogHeaderFrame.h \
	src/DoYouWantToInitDialog.h \
	src/DropDownListFrame.h \
	src/EditGitIgnoreDialog.h \
	src/EditProfilesDialog.h \
	src/EditRemoteDialog.h \
	src/EditTagsDialog.h \
	src/ExperimentDialog.h \
	src/FileDiffSliderWidget.h \
	src/FileDiffWidget.h \
	src/FileHistoryWindow.h \
	src/FileListWidget.h \
	src/FilePropertyDialog.h \
	src/FileUtil.h \
	src/FindCommitDialog.h \
	src/GenerateCommitMessageDialog.h \
	src/GenerateCommitMessageThread.h \
	src/GeneratedCommitMessage.h \
	src/GenerativeAI.h \
	src/GenerativeAI.h \
	src/Git.h \
	src/GitBasicSession.h \
	src/GitCommandCache.h \
	src/GitCommandRunner.h \
	src/GitConfigGlobalAddSafeDirectoryDialog.h \
	src/GitHubAPI.h \
	src/GitObjectManager.h \
	src/GitPack.h \
	src/GitPackIdxV2.h \
	src/GitProcessThread.h \
	src/GitTypes.h \
	src/HyperLinkLabel.h \
	src/ImageViewWidget.h \
	src/IncrementalSearch.h \
	src/InputNewTagDialog.h \
	src/JumpDialog.h \
	src/Languages.h \
	src/LineEditDialog.h \
	src/LocalSocketReader.h \
	src/MainWindow.h \
	src/ManageWorkingFolderDialog.h \
	src/MaximizeButton.h \
	src/MemoryReader.h \
	src/MenuButton.h \
	src/MergeDialog.h \
	src/MyImageViewWidget.h \
	src/MyProcess.h \
	src/MySettings.h \
	src/MyTableWidgetDelegate.h \
	src/MyTextEditorWidget.h \
	src/MyToolButton.h \
	src/OverrideWaitCursor.h \
	src/Photoshop.h \
	src/Profile.h \
	src/ProgressTextLabel.h \
	src/ProgressWidget.h \
	src/PushDialog.h \
	src/ReadOnlyLineEdit.h \
	src/ReadOnlyPlainTextEdit.h \
	src/ReflogWindow.h \
	src/RemoteAdvancedOptionWidget.h \
	src/RemoteRepositoriesTableWidget.h \
	src/RepositoryInfo.h \
	src/RepositoryInfoFrame.h \
	src/RepositoryModel.h \
	src/RepositoryPropertyDialog.h \
	src/RepositorySearchResultItem.h \
	src/RepositoryTreeWidget.h \
	src/RepositoryUrlLineEdit.h \
	src/SearchFromGitHubDialog.h \
	src/SelectCommandDialog.h \
	src/SelectGpgKeyDialog.h \
	src/SelectItemDialog.h \
	src/SetGlobalUserDialog.h \
	src/SetGpgSigningDialog.h \
	src/SettingAiForm.h \
	src/SettingBehaviorForm.h \
	src/SettingExampleForm.h \
	src/SettingGeneralForm.h \
	src/SettingNetworkForm.h \
	src/SettingOptionsForm.h \
	src/SettingPrograms2Form.h \
	src/SettingProgramsForm.h \
	src/SettingVisualForm.h \
	src/SettingWorkingFolderForm.h \
	src/SettingsDialog.h \
	src/SimpleImageWidget.h \
	src/StatusInfo.h \
	src/StatusLabel.h \
	src/SubmoduleAddDialog.h \
	src/SubmoduleUpdateDialog.h \
	src/SubmodulesDialog.h \
	src/Terminal.h \
	src/TextEditDialog.h \
	src/Theme.h \
	src/UserEvent.h \
	src/Util.h \
	src/WelcomeWizardDialog.h \
	src/WorkingDirLineEdit.h \
	src/XmlTagState.h \
	src/coloredit/ColorDialog.h \
	src/coloredit/ColorEditWidget.h \
	src/coloredit/ColorPreviewWidget.h \
	src/coloredit/ColorSlider.h \
	src/coloredit/ColorSquareWidget.h \
	src/coloredit/RingSlider.h \
	src/common/base64.h \
	src/common/charvec.h \
	src/common/joinpath.h \
	src/common/jstream.h \
	src/common/misc.h \
	src/common/strformat.h \
	src/darktheme/DarkStyle.h \
	src/darktheme/LightStyle.h \
	src/darktheme/NinePatch.h \
	src/darktheme/TraditionalWindowsStyleTreeControl.h \
	src/dtl/Diff.hpp \
	src/dtl/Diff3.hpp \
	src/dtl/Lcs.hpp \
	src/dtl/Sequence.hpp \
	src/dtl/Ses.hpp \
	src/dtl/dtl.hpp \
	src/dtl/functors.hpp \
	src/dtl/variables.hpp \
	src/gpg.h \
	src/platform.h \
	src/texteditor/AbstractCharacterBasedApplication.h \
	src/texteditor/InputMethodPopup.h \
	src/texteditor/TextEditorTheme.h \
	src/texteditor/TextEditorView.h \
	src/texteditor/TextEditorWidget.h \
	src/texteditor/UnicodeWidth.h \
	src/texteditor/unicode.h \
	src/urlencode.h \
	src/webclient.h \
	src/zip/zip.h \
	src/zip/zipinternal.h

HEADERS += version.h

FORMS += \
    $$PWD/src/SettingLoggingForm.ui \
    src/AboutDialog.ui \
	src/AddRepositoriesCollectivelyDialog.ui \
	src/AddRepositoryDialog.ui \
	src/AreYouSureYouWantToContinueConnectingDialog.ui \
	src/BigDiffWindow.ui \
	src/BlameWindow.ui \
	src/CheckoutDialog.ui \
	src/CherryPickDialog.ui \
	src/CleanSubModuleDialog.ui \
	src/CloneFromGitHubDialog.ui \
	src/CommitDialog.ui \
	src/CommitExploreWindow.ui \
	src/CommitPropertyDialog.ui \
	src/CommitViewWindow.ui \
	src/ConfigCredentialHelperDialog.ui \
	src/ConfigSigningDialog.ui \
	src/ConfigUserDialog.ui \
	src/CreateRepositoryDialog.ui \
	src/DeleteBranchDialog.ui \
	src/DeleteTagsDialog.ui \
	src/DoYouWantToInitDialog.ui \
	src/EditGitIgnoreDialog.ui \
	src/EditProfilesDialog.ui \
	src/EditRemoteDialog.ui \
	src/EditTagsDialog.ui \
	src/ExperimentDialog.ui \
	src/FileDiffWidget.ui \
	src/FileHistoryWindow.ui \
	src/FilePropertyDialog.ui \
	src/FindCommitDialog.ui \
	src/GenerateCommitMessageDialog.ui \
	src/GitConfigGlobalAddSafeDirectoryDialog.ui \
	src/InputNewTagDialog.ui \
	src/JumpDialog.ui \
	src/LineEditDialog.ui \
	src/MainWindow.ui \
	src/ManageWorkingFolderDialog.ui \
	src/MergeDialog.ui \
	src/ProgressWidget.ui \
	src/PushDialog.ui \
	src/ReflogWindow.ui \
	src/RemoteAdvancedOptionWidget.ui \
	src/RepositoryPropertyDialog.ui \
	src/SearchFromGitHubDialog.ui \
	src/SelectCommandDialog.ui \
	src/SelectGpgKeyDialog.ui \
	src/SelectItemDialog.ui \
	src/SetGlobalUserDialog.ui \
	src/SetGpgSigningDialog.ui \
	src/SettingAiForm.ui \
	src/SettingBehaviorForm.ui \
	src/SettingExampleForm.ui \
	src/SettingGeneralForm.ui \
	src/SettingNetworkForm.ui \
	src/SettingOptionsForm.ui \
	src/SettingPrograms2Form.ui \
	src/SettingProgramsForm.ui \
	src/SettingVisualForm.ui \
	src/SettingWorkingFolderForm.ui \
	src/SettingsDialog.ui \
	src/SubmoduleAddDialog.ui \
	src/SubmoduleUpdateDialog.ui \
	src/SubmodulesDialog.ui \
	src/TextEditDialog.ui \
	src/WelcomeWizardDialog.ui \
	src/coloredit/ColorDialog.ui \
	src/coloredit/ColorEditWidget.ui

RESOURCES += \
	src/resources/resources.qrc

unix {
	SOURCES += \
		src/unix/UnixUtil.cpp \
		src/unix/UnixProcess.cpp \
		src/unix/UnixPtyProcess.cpp
	HEADERS += \
		src/unix/UnixUtil.h \
		src/unix/UnixProcess.h \
		src/unix/UnixPtyProcess.h
}

win32 {
	SOURCES += \
	src/win32/Win32Util.cpp \
		src/win32/Win32Process.cpp \
        src/win32/Win32PtyProcess.cpp \
        src/win32/event.cpp \
        src/win32/thread.cpp

	HEADERS += \
	src/win32/Win32Util.h \
		src/win32/Win32Process.h \
        src/win32/Win32PtyProcess.h \
        src/win32/event.h \
        src/win32/mutex.h \
        src/win32/thread.h

    LIBS += -lole32
}

unsafe {
    SOURCES += \
	    src/sshsupport/ConfirmRemoteSessionDialog.cpp \
		src/sshsupport/GitRemoteSshSession.cpp \
		src/sshsupport/Quissh.cpp \
		src/sshsupport/SshDialog.cpp
	HEADERS += \
	    src/sshsupport/ConfirmRemoteSessionDialog.h \
		src/sshsupport/GitRemoteSshSession.h \
		src/sshsupport/Quissh.h \
		src/sshsupport/SshDialog.h
	FORMS += \
	    src/sshsupport/ConfirmRemoteSessionDialog.ui \
		src/sshsupport/SshDialog.ui
}

include(migemo.pri)
