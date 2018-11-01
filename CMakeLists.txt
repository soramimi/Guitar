cmake_minimum_required(VERSION 3.2)

project(Guitar
    VERSION 0.9.0
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

set(Guitar_SOURCES
    version.c
    src/main.cpp
    src/MainWindow.cpp
    src/Git.cpp
    src/common/joinpath.cpp
    src/common/misc.cpp
    src/ConfigCredentialHelperDialog.cpp
    src/MySettings.cpp
    src/FileDiffWidget.cpp
    src/TextEditDialog.cpp
    src/LogTableWidget.cpp
    src/FileDiffSliderWidget.cpp
    src/FileUtil.cpp
    src/SettingsDialog.cpp
    src/MergeBranchDialog.cpp
    src/CloneDialog.cpp
    src/AboutDialog.cpp
    src/RepositoryInfoFrame.cpp
    src/RepositoryPropertyDialog.cpp
    src/RepositoryData.cpp
    src/MyToolButton.cpp
    src/GitDiff.cpp
    src/CommitPropertyDialog.cpp
    src/Terminal.cpp
    src/DeleteTagsDialog.cpp
    src/RepositoriesTreeWidget.cpp
    src/SelectCommandDialog.cpp
    src/ImageViewWidget.cpp
    src/FileHistoryWindow.cpp
    src/GitPackIdxV2.cpp
    src/GitPack.cpp
    src/GitObjectManager.cpp
    src/FilePropertyDialog.cpp
    src/BigDiffWindow.cpp
    src/MaximizeButton.cpp
    src/CommitExploreWindow.cpp
    src/ReadOnlyLineEdit.cpp
    src/ReadOnlyPlainTextEdit.cpp
    src/MyTableWidgetDelegate.cpp
    src/SetRemoteUrlDialog.cpp
    src/ClearButton.cpp
    src/SetUserDialog.cpp
    src/SearchFromGitHubDialog.cpp
    src/webclient.cpp
    src/charvec.cpp
    src/urlencode.cpp
    src/HyperLinkLabel.cpp
    src/JumpDialog.cpp
    src/CheckoutDialog.cpp
    src/DeleteBranchDialog.cpp
    src/BasicRepositoryDialog.cpp
    src/RemoteRepositoriesTableWidget.cpp
    src/LocalSocketReader.cpp
    src/PushDialog.cpp
    src/StatusLabel.cpp
    src/RepositoryLineEdit.cpp
    src/DirectoryLineEdit.cpp
    src/AbstractSettingForm.cpp
    src/SettingExampleForm.cpp
    src/CreateRepositoryDialog.cpp
    src/GitHubAPI.cpp
    src/MemoryReader.cpp
    src/ExperimentDialog.cpp
    src/gunzip.cpp
    src/AvatarLoader.cpp
    src/SettingNetworkForm.cpp
    src/Photoshop.cpp
    src/SettingBehaviorForm.cpp
    src/MyProcess.cpp
    src/FileViewWidget.cpp
    src/MyTextEditorWidget.cpp
    src/AbstractProcess.cpp
    src/texteditor/AbstractCharacterBasedApplication.cpp
    src/texteditor/InputMethodPopup.cpp
    src/texteditor/TextEditorTheme.cpp
    src/texteditor/TextEditorWidget.cpp
    src/texteditor/unicode.cpp
    src/texteditor/UnicodeWidth.cpp
    src/MyImageViewWidget.cpp
    src/SetGlobalUserDialog.cpp
    src/ReflogWindow.cpp
    src/Theme.cpp
    src/ApplicationGlobal.cpp
    src/BlameWindow.cpp
    src/MenuButton.cpp
    src/SettingGeneralForm.cpp
    src/WelcomeWizardDialog.cpp
    src/DialogHeaderFrame.cpp
    src/CommitViewWindow.cpp
    src/EditRemoteDialog.cpp
    src/gpg.cpp
    src/SelectGpgKeyDialog.cpp
    src/SetGpgSigningDialog.cpp
    src/CommitDialog.cpp
    src/ConfigSigningDialog.cpp
    src/AreYouSureYouWantToContinueConnectingDialog.cpp
    src/LineEditDialog.cpp
    src/SettingProgramsForm.cpp
    src/InputNewTagDialog.cpp
    src/EditTagsDialog.cpp
    ./darktheme/src/DarkStyle.cpp
    ./darktheme/src/NinePatch.cpp
    ./darktheme/src/StandardStyle.cpp
    ./darktheme/src/TraditionalWindowsStyleTreeControl.cpp
    src/EditGitIgnoreDialog.cpp
    src/SelectItemDialog.cpp
    src/RebaseOntoDialog.cpp
    )


set(Guitar_HEADERS
    src/MainWindow.h
    src/Git.h
    src/common/joinpath.h
    src/common/misc.h
    src/ConfigCredentialHelperDialog.h
    src/MySettings.h
    src/main.h
    src/FileDiffWidget.h
    src/TextEditDialog.h
    src/LogTableWidget.h
    src/FileDiffSliderWidget.h
    src/FileUtil.h
    src/SettingsDialog.h
    src/MergeBranchDialog.h
    src/CloneDialog.h
    src/AboutDialog.h
    src/RepositoryInfoFrame.h
    src/RepositoryPropertyDialog.h
    src/RepositoryData.h
    src/MyToolButton.h
    src/GitDiff.h
    src/CommitPropertyDialog.h
    src/Terminal.h
    src/DeleteTagsDialog.h
    src/RepositoriesTreeWidget.h
    src/SelectCommandDialog.h
    src/ImageViewWidget.h
    src/FileHistoryWindow.h
    src/Debug.h
    src/GitPackIdxV2.h
    src/GitPack.h
    src/GitObjectManager.h
    src/FilePropertyDialog.h
    src/BigDiffWindow.h
    src/MaximizeButton.h
    src/CommitExploreWindow.h
    src/ReadOnlyLineEdit.h
    src/ReadOnlyPlainTextEdit.h
    src/MyTableWidgetDelegate.h
    src/SetRemoteUrlDialog.h
    myzlib.h
    src/ClearButton.h
    src/SetUserDialog.h
    src/SearchFromGitHubDialog.h
    src/webclient.h
    src/charvec.h
    src/urlencode.h
    src/HyperLinkLabel.h
    src/JumpDialog.h
    src/CheckoutDialog.h
    src/DeleteBranchDialog.h
    src/BasicRepositoryDialog.h
    src/RemoteRepositoriesTableWidget.h
    src/LocalSocketReader.h
    src/PushDialog.h
    src/StatusLabel.h
    src/RepositoryLineEdit.h
    src/DirectoryLineEdit.h
    src/AbstractSettingForm.h
    src/SettingExampleForm.h
    src/CreateRepositoryDialog.h
    src/GitHubAPI.h
    src/MemoryReader.h
    src/ExperimentDialog.h
    src/gunzip.h
    src/AvatarLoader.h
    src/SettingNetworkForm.h
    src/Photoshop.h
    src/SettingBehaviorForm.h
    src/MyProcess.h
    src/MyTextEditorWidget.h
    src/AbstractProcess.h
    src/texteditor/AbstractCharacterBasedApplication.h
    src/texteditor/InputMethodPopup.h
    src/texteditor/TextEditorTheme.h
    src/texteditor/TextEditorWidget.h
    src/texteditor/unicode.h
    src/texteditor/UnicodeWidth.h
    src/MyImageViewWidget.h
    src/SetGlobalUserDialog.h
    src/ReflogWindow.h
    src/Theme.h
    src/ApplicationGlobal.h
    src/BlameWindow.h
    src/MenuButton.h
    src/SettingGeneralForm.h
    src/WelcomeWizardDialog.h
    src/DialogHeaderFrame.h
    src/CommitViewWindow.h
    src/EditRemoteDialog.h
    src/gpg.h
    src/SelectGpgKeyDialog.h
    src/SetGpgSigningDialog.h
    src/CommitDialog.h
    src/ConfigSigningDialog.h
    src/AreYouSureYouWantToContinueConnectingDialog.h
    src/LineEditDialog.h
    src/SettingProgramsForm.h
    src/InputNewTagDialog.h
    src/EditTagsDialog.h
    ./darktheme/src/DarkStyle.h
    ./darktheme/src/NinePatch.h
    ./darktheme/src/StandardStyle.h
    ./darktheme/src/TraditionalWindowsStyleTreeControl.h
    src/EditGitIgnoreDialog.h
    src/SelectItemDialog.h
    src/RebaseOntoDialog.h
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
        src/win32/thread.cpp
        src/win32/event.cpp
        src/win32/win32.cpp
        src/win32/Win32Process.cpp
        src/win32/Win32PtyProcess.cpp
        )
    list(APPEND Guitar_HEADERS
        src/win32/thread.h
        src/win32/event.h
        src/win32/mutex.h
        src/win32/win32.h
        src/win32/Win32Process.h
        src/win32/Win32PtyProcess.h
        )
endif()

set(Guitar_UIS
    src/MainWindow.ui
    src/ConfigCredentialHelperDialog.ui
    src/TextEditDialog.ui
    src/SettingsDialog.ui
    src/MergeBranchDialog.ui
    src/CloneDialog.ui
    src/AboutDialog.ui
    src/RepositoryPropertyDialog.ui
    src/CommitPropertyDialog.ui
    src/DeleteTagsDialog.ui
    src/SelectCommandDialog.ui
    src/FileDiffWidget.ui
    src/FileHistoryWindow.ui
    src/FilePropertyDialog.ui
    src/BigDiffWindow.ui
    src/CommitExploreWindow.ui
    src/SetRemoteUrlDialog.ui
    src/SetUserDialog.ui
    src/SearchFromGitHubDialog.ui
    src/JumpDialog.ui
    src/CheckoutDialog.ui
    src/DeleteBranchDialog.ui
    src/PushDialog.ui
    src/SettingExampleForm.ui
    src/CreateRepositoryDialog.ui
    src/ExperimentDialog.ui
    src/SettingNetworkForm.ui
    src/SettingBehaviorForm.ui
    src/SetGlobalUserDialog.ui
    src/ReflogWindow.ui
    src/BlameWindow.ui
    src/SettingGeneralForm.ui
    src/WelcomeWizardDialog.ui
    src/CommitViewWindow.ui
    src/EditRemoteDialog.ui
    src/SelectGpgKeyDialog.ui
    src/SetGpgSigningDialog.ui
    src/CommitDialog.ui
    src/ConfigSigningDialog.ui
    src/AreYouSureYouWantToContinueConnectingDialog.ui
    src/LineEditDialog.ui
    src/SettingProgramsForm.ui
    src/InputNewTagDialog.ui
    src/EditTagsDialog.ui
    src/EditGitIgnoreDialog.ui
    src/SelectItemDialog.ui
    src/RebaseOntoDialog.ui
    )

set(Guitar_RESOURCES
    resources/resources.qrc
    )

set(Guitar_TRANSLATIONS
    translations/Guitar_ja.ts
    )

qt5_add_translation(Guitar_QM_FILES ${Guitar_TRANSLATIONS})

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
