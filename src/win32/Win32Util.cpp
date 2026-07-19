
#include "Win32Util.h"
#include "ApplicationGlobal.h"
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtGlobal>
#include <ShlObj.h>
#include <Windows.h>
#include <common/joinpath.h>
#include <commoncontrols.h>
#include <deque>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#define FAILED_(TEXT) throw std::string(TEXT)

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtWinExtras/QtWinExtras>

QPixmap pixmapFromHICON(HICON hIcon)
{
	return QtWin::fromHICON(hIcon);
}

#else

QPixmap pixmapFromHICON(HICON hIcon)
{
	return QPixmap::fromImage(QImage::fromHICON(hIcon));
}

#endif


std::wstring replace_slash_to_backslash(std::wstring const &str)
{
	std::wstring sb;
	for (size_t i = 0; i < str.size(); i++) {
		wchar_t c = str[i];
		if (c == '/') {
			c = '\\';
		}
		sb += c;
	}
	return sb;
}



QString getModuleFileName()
{
	wchar_t tmp[300];
	DWORD n = GetModuleFileNameW(nullptr, tmp, 300);
	return QString::fromUtf16((ushort const *)tmp, n);
}

QString getAppDataLocation()
{
	wchar_t tmp[300];
	if (SHGetSpecialFolderPathW(nullptr, tmp, CSIDL_APPDATA, TRUE)) {
		return QString::fromUtf16((ushort const *)tmp);
	}
	return QString();
}


void setEnvironmentVariable(QString const &name, QString const &value)
{
	SetEnvironmentVariableW((wchar_t const *)name.utf16(), (wchar_t const *)value.utf16());
}


QString getWin32HttpProxy()
{
	HKEY hk = nullptr;
	auto Close = [&](){
		RegCloseKey(hk);
	};
	try {
		LSTATUS s;
		s = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hk);
		if (s != ERROR_SUCCESS) throw s;
		char tmp[1000];
		DWORD type = 0;
		DWORD len = sizeof(tmp);
		s = RegQueryValueExA(hk, "ProxyServer", nullptr, &type, (unsigned char *)tmp, &len);
		if (s != ERROR_SUCCESS) throw s;
		while (len > 0 && tmp[len - 1] == 0) {
			len--;
		}
		Close();
		return QString::fromLatin1(tmp, len);
	} catch (LSTATUS s) {
		Close();
		(void)s;
	}
	return QString();
}




namespace {
QIcon iconFromExtension_(QString const &ext, UINT flag)
{
	QIcon icon;
	QString name = "*." + ext;
	SHFILEINFOW shinfo;
	if (SHGetFileInfoW((wchar_t const *)name.utf16(), 0, &shinfo, sizeof(shinfo), flag | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES) != 0) {
		if (shinfo.hIcon) {
			QPixmap pm = pixmapFromHICON(shinfo.hIcon);
			if (!pm.isNull()) {
				icon = QIcon(pm);
			}
			DestroyIcon(shinfo.hIcon);
		}
	}
	return icon;
}
}

QIcon winIconFromExtensionLarge(QString const &ext)
{
	return iconFromExtension_(ext, SHGFI_LARGEICON);
}

QIcon winIconFromExtensionSmall(QString const &ext)
{
	return iconFromExtension_(ext, SHGFI_SMALLICON);
}



//

void createWin32Shortcut(Win32ShortcutData const &data)
{
	Win32ShortcutData d = data;

	d.lnkpath = replace_slash_to_backslash(d.lnkpath);

	HRESULT hr;
	IShellLink *shlink = 0;
	IShellLinkDataList *psldl = 0;
	IPersistFile *pfile = 0;

	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&shlink);
	if (SUCCEEDED(hr)) {
		shlink->SetPath(d.targetpath.c_str());
		if (!d.arguments.empty()) {
			shlink->SetArguments(d.arguments.c_str());
		}
		if (!d.workingdir.empty()) {
			shlink->SetWorkingDirectory(d.workingdir.c_str());
		}
		if (!d.iconpath.empty()) {
			shlink->SetIconLocation(d.iconpath.c_str(), d.iconindex);
		}

		if (d.runas) {
			hr = shlink->QueryInterface(IID_IShellLinkDataList, (void **)&psldl);
			if (SUCCEEDED(hr)) {
				psldl->SetFlags(SLDF_RUNAS_USER);
			}
		}

		hr = shlink->QueryInterface(IID_IPersistFile, (void**)&pfile);
		if (SUCCEEDED(hr)) {
			hr = pfile->Save(d.lnkpath.c_str(), TRUE);
			(void)hr;
			pfile->Release();
		}

		shlink->Release();
	}
}

namespace platform {

void createApplicationShortcut(QWidget *parent)
{
	QString desktop_dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	QString target_path = QApplication::applicationFilePath();

	Win32ShortcutData data;
	data.targetpath = (wchar_t const *)target_path.utf16();

	QString launcher_dir = desktop_dir;
	QString name = APPLICATION_NAME;
	QString iconpath = target_path;//icon_dir / name + ".ico";
	QString launcher_path = launcher_dir / name + ".lnk";
	QString lnkpath = QFileDialog::getSaveFileName(parent, QApplication::tr("Save Launcher File"), launcher_path, "Launcher files (*.lnk)");
	data.iconpath = (wchar_t const *)iconpath.utf16();
	data.lnkpath = (wchar_t const *)lnkpath.utf16();

	if (!launcher_path.isEmpty()) {
		createWin32Shortcut(data);
	}
}

} // namespace platform

