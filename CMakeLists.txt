cmake_minimum_required(VERSION 3.2)

project(Guitar
    VERSION 0.9.3
    LANGUAGES CXX C
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(Qt5 COMPONENTS Core Widgets Network Svg REQUIRED)
find_package(zlib REQUIRED )
find_package(OpenSSL REQUIRED )
find_package(Qt5LinguistTools REQUIRED)
if(WIN32)
    # check package at
    # https://github.com/rprichard/winpty
    find_package(winpty REQUIRED )
endif()

# extract version information
string(TIMESTAMP Guitar_copyright_year "%Y")
execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE Guitar_git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_options(-std=c++11)

configure_file(version.h.in version.h)
configure_file(win.rc.in win.rc.h)
configure_file(Info.plist.in Info.plist)

set(Guitar_SOURCES
	src/AboutDialog.cpp
	src/AbstractProcess.cpp
	src/AbstractSettingForm.cpp
	src/ApplicationGlobal.cpp
	src/AreYouSureYouWantToContinueConnectingDialog.cpp
	src/AvatarLoader.cpp
	src/BasicMainWindow.cpp
	src/BasicRepositoryDialog.cpp
	src/BigDiffWindow.cpp
	src/BlameWindow.cpp
	src/CheckoutDialog.cpp
	src/ClearButton.cpp
	src/CloneDialog.cpp
	src/CloneFromGitHubDialog.cpp
	src/CommitDialog.cpp
	src/CommitExploreWindow.cpp
	src/CommitPropertyDialog.cpp
	src/CommitViewWindow.cpp
	src/ConfigCredentialHelperDialog.cpp
	src/ConfigSigningDialog.cpp
	src/CreateRepositoryDialog.cpp
	src/DeleteBranchDialog.cpp
	src/DeleteTagsDialog.cpp
	src/DialogHeaderFrame.cpp
	src/DirectoryLineEdit.cpp
	src/DoYouWantToInitDialog.cpp
	src/EditGitIgnoreDialog.cpp
	src/EditRemoteDialog.cpp
	src/EditTagsDialog.cpp
	src/ExperimentDialog.cpp
	src/FileDiffSliderWidget.cpp
	src/FileDiffWidget.cpp
	src/FileHistoryWindow.cpp
	src/FilePropertyDialog.cpp
	src/FileUtil.cpp
	src/FileViewWidget.cpp
	src/Git.cpp
	src/GitDiff.cpp
	src/GitHubAPI.cpp
	src/GitObjectManager.cpp
	src/GitPack.cpp
	src/GitPackIdxV2.cpp
	src/HyperLinkLabel.cpp
	src/ImageViewWidget.cpp
	src/InputNewTagDialog.cpp
	src/JumpDialog.cpp
	src/LineEditDialog.cpp
	src/LocalSocketReader.cpp
	src/LogTableWidget.cpp
	src/MainWindow.cpp
	src/MaximizeButton.cpp
	src/MemoryReader.cpp
	src/MenuButton.cpp
	src/MergeBranchDialog.cpp
	src/MyImageViewWidget.cpp
	src/MyProcess.cpp
	src/MySettings.cpp
	src/MyTableWidgetDelegate.cpp
	src/MyTextEditorWidget.cpp
	src/MyToolButton.cpp
    src/ObjectBrowserDialog.cpp
	src/Photoshop.cpp
	src/PushDialog.cpp
	src/ReadOnlyLineEdit.cpp
	src/ReadOnlyPlainTextEdit.cpp
	src/ReflogWindow.cpp
	src/RemoteRepositoriesTableWidget.cpp
	src/RemoteWatcher.cpp
	src/RepositoriesTreeWidget.cpp
	src/RepositoryData.cpp
	src/RepositoryInfoFrame.cpp
	src/RepositoryLineEdit.cpp
	src/RepositoryPropertyDialog.cpp
	src/SearchFromGitHubDialog.cpp
	src/SelectCommandDialog.cpp
	src/SelectGpgKeyDialog.cpp
	src/SelectItemDialog.cpp
	src/SetGlobalUserDialog.cpp
	src/SetGpgSigningDialog.cpp
	src/SetRemoteUrlDialog.cpp
	src/SetUserDialog.cpp
	src/SettingBehaviorForm.cpp
	src/SettingExampleForm.cpp
	src/SettingGeneralForm.cpp
	src/SettingNetworkForm.cpp
	src/SettingProgramsForm.cpp
	src/SettingsDialog.cpp
	src/StatusLabel.cpp
	src/Terminal.cpp
	src/TextEditDialog.cpp
	src/Theme.cpp
	src/WelcomeWizardDialog.cpp
	src/charvec.cpp
	src/common/joinpath.cpp
	src/common/misc.cpp
	src/darktheme/DarkStyle.cpp
	src/darktheme/NinePatch.cpp
	src/darktheme/StandardStyle.cpp
	src/darktheme/TraditionalWindowsStyleTreeControl.cpp
	src/gpg.cpp
	src/gunzip.cpp
	src/main.cpp
	src/texteditor/AbstractCharacterBasedApplication.cpp
	src/texteditor/InputMethodPopup.cpp
	src/texteditor/TextEditorTheme.cpp
	src/texteditor/TextEditorWidget.cpp
	src/texteditor/UnicodeWidth.cpp
	src/texteditor/unicode.cpp
	src/urlencode.cpp
	src/webclient.cpp
    )


set(Guitar_HEADERS
	src/AboutDialog.h
	src/AbstractProcess.h
	src/AbstractSettingForm.h
	src/ApplicationGlobal.h
	src/AreYouSureYouWantToContinueConnectingDialog.h
	src/AvatarLoader.h
	src/BasicMainWindow.h
	src/BasicRepositoryDialog.h
	src/BigDiffWindow.h
	src/BlameWindow.h
	src/CheckoutDialog.h
	src/ClearButton.h
	src/CloneDialog.h
	src/CloneFromGitHubDialog.h
	src/CommitDialog.h
	src/CommitExploreWindow.h
	src/CommitPropertyDialog.h
	src/CommitViewWindow.h
	src/ConfigCredentialHelperDialog.h
	src/ConfigSigningDialog.h
	src/CreateRepositoryDialog.h
	src/Debug.h
	src/DeleteBranchDialog.h
	src/DeleteTagsDialog.h
	src/DialogHeaderFrame.h
	src/DirectoryLineEdit.h
	src/DoYouWantToInitDialog.h
	src/EditGitIgnoreDialog.h
	src/EditRemoteDialog.h
	src/EditTagsDialog.h
	src/ExperimentDialog.h
	src/FileDiffSliderWidget.h
	src/FileDiffWidget.h
	src/FileHistoryWindow.h
	src/FilePropertyDialog.h
	src/FileUtil.h
	src/FileViewWidget.h
	src/Git.h
	src/GitDiff.h
	src/GitHubAPI.h
	src/GitObjectManager.h
	src/GitPack.h
	src/GitPackIdxV2.h
	src/HyperLinkLabel.h
	src/ImageViewWidget.h
	src/InputNewTagDialog.h
	src/JumpDialog.h
	src/LineEditDialog.h
	src/LocalSocketReader.h
	src/LogTableWidget.h
	src/MainWindow.h
	src/MaximizeButton.h
	src/MemoryReader.h
	src/MenuButton.h
	src/MergeBranchDialog.h
	src/MyImageViewWidget.h
	src/MyProcess.h
	src/MySettings.h
	src/MyTableWidgetDelegate.h
	src/MyTextEditorWidget.h
	src/MyToolButton.h
    src/ObjectBrowserDialog.h
	src/Photoshop.h
	src/PushDialog.h
	src/ReadOnlyLineEdit.h
	src/ReadOnlyPlainTextEdit.h
	src/ReflogWindow.h
	src/RemoteRepositoriesTableWidget.h
	src/RemoteWatcher.h
	src/RepositoriesTreeWidget.h
	src/RepositoryData.h
	src/RepositoryInfoFrame.h
	src/RepositoryLineEdit.h
	src/RepositoryPropertyDialog.h
	src/SearchFromGitHubDialog.h
	src/SelectCommandDialog.h
	src/SelectGpgKeyDialog.h
	src/SelectItemDialog.h
	src/SetGlobalUserDialog.h
	src/SetGpgSigningDialog.h
	src/SetRemoteUrlDialog.h
	src/SetUserDialog.h
	src/SettingBehaviorForm.h
	src/SettingExampleForm.h
	src/SettingGeneralForm.h
	src/SettingNetworkForm.h
	src/SettingProgramsForm.h
	src/SettingsDialog.h
	src/StatusLabel.h
	src/Terminal.h
	src/TextEditDialog.h
	src/Theme.h
	src/WelcomeWizardDialog.h
	src/charvec.h
	src/common/joinpath.h
	src/common/misc.h
	src/darktheme/DarkStyle.h
	src/darktheme/NinePatch.h
	src/darktheme/StandardStyle.h
	src/darktheme/TraditionalWindowsStyleTreeControl.h
	src/gpg.h
	src/gunzip.h
	src/main.h
	src/texteditor/AbstractCharacterBasedApplication.h
	src/texteditor/InputMethodPopup.h
	src/texteditor/TextEditorTheme.h
	src/texteditor/TextEditorWidget.h
	src/texteditor/UnicodeWidth.h
	src/texteditor/unicode.h
	src/urlencode.h
	src/webclient.h
    )

if(UNIX)
    list(APPEND Guitar_SOURCES
        src/unix/UnixProcess.cpp
        src/unix/UnixPtyProcess.cpp
        )
    list(APPEND Guitar_HEADERS
        src/unix/UnixProcess.h
        src/unix/UnixPtyProcess.h
        )
elseif(WIN32)
    list(APPEND Guitar_SOURCES
		src/win32/Win32Process.cpp
		src/win32/Win32PtyProcess.cpp
		src/win32/event.cpp
		src/win32/thread.cpp
		src/win32/win32.cpp
        )
    list(APPEND Guitar_HEADERS
		src/win32/Win32Process.h
		src/win32/Win32PtyProcess.h
		src/win32/event.h
		src/win32/mutex.h
		src/win32/thread.h
		src/win32/win32.h
        )
endif()

set(Guitar_UIS
	src/AboutDialog.ui
	src/AreYouSureYouWantToContinueConnectingDialog.ui
	src/BigDiffWindow.ui
	src/BlameWindow.ui
	src/CheckoutDialog.ui
	src/CloneDialog.ui
	src/CloneFromGitHubDialog.ui
	src/CommitDialog.ui
	src/CommitExploreWindow.ui
	src/CommitPropertyDialog.ui
	src/CommitViewWindow.ui
	src/ConfigCredentialHelperDialog.ui
	src/ConfigSigningDialog.ui
	src/CreateRepositoryDialog.ui
	src/DeleteBranchDialog.ui
	src/DeleteTagsDialog.ui
	src/DoYouWantToInitDialog.ui
	src/EditGitIgnoreDialog.ui
	src/EditRemoteDialog.ui
	src/EditTagsDialog.ui
	src/ExperimentDialog.ui
	src/FileDiffWidget.ui
	src/FileHistoryWindow.ui
	src/FilePropertyDialog.ui
	src/InputNewTagDialog.ui
	src/JumpDialog.ui
	src/LineEditDialog.ui
	src/MainWindow.ui
	src/MergeBranchDialog.ui
    src/ObjectBrowserDialog.ui
	src/PushDialog.ui
	src/ReflogWindow.ui
	src/RepositoryPropertyDialog.ui
	src/SearchFromGitHubDialog.ui
	src/SelectCommandDialog.ui
	src/SelectGpgKeyDialog.ui
	src/SelectItemDialog.ui
	src/SetGlobalUserDialog.ui
	src/SetGpgSigningDialog.ui
	src/SetRemoteUrlDialog.ui
	src/SetUserDialog.ui
	src/SettingBehaviorForm.ui
	src/SettingExampleForm.ui
	src/SettingGeneralForm.ui
	src/SettingNetworkForm.ui
	src/SettingProgramsForm.ui
	src/SettingsDialog.ui
	src/TextEditDialog.ui
	src/WelcomeWizardDialog.ui
    )

set(Guitar_RESOURCES
    src/resources/resources.qrc
    )

set(Guitar_TRANSLATIONS
	src/resources/translations/Guitar_ja.ts
    )

qt5_add_translation(Guitar_QM_FILES ${Guitar_TRANSLATIONS})
configure_file(${Guitar_RESOURCES} ${CMAKE_BINARY_DIR} COPYONLY)

add_custom_target(translations ALL DEPENDS ${Guitar_QM_FILES})

add_custom_target(resources ALL DEPENDS ${Guitar_RESOURCES})

add_dependencies(resources translations)

include_directories(src)
include_directories(src/texteditor)

add_executable(${PROJECT_NAME}
    ${Guitar_SOURCES}
    ${Guitar_HEADERS}
    ${Guitar_UIS}
    ${Guitar_RESOURCES}
    ${Guitar_QM_FILES}
    )

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Widgets
    Qt5::Network
    Qt5::Svg
    zlib
    OpenSSL::SSL OpenSSL::Crypto
)


add_subdirectory(extra)
