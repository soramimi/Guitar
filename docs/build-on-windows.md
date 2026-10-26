# Building Guitar on Windows

This guide explains how to build Guitar on Windows using Visual Studio (MSVC) and Qt.

Guitar consists of three projects, all of which must be built:

| Project | Location |
|---|---|
| FileTypePlugin (libmagic based file type detection) | `subprojects/FileTypePlugin` |
| IncrementalSearchPlugin (MeCab based incremental search) | `subprojects/IncrementalSearchPlugin` |
| Guitar (main application) | repository root |

All outputs are placed in `_bin`.

## Prerequisites

- **Visual Studio 2022** with C++ development tools (Community Edition is fine)
- **Qt 6.11.0** (MSVC 2022 64-bit) and **Qt Creator** tools (for `jom`)
  - Default location: `C:\Qt\6.11.0\msvc2022_64` and `C:\Qt\Tools\QtCreator\bin\jom\jom.exe`
- **Ruby for Windows**
- **Git for Windows**
- **7-Zip** (`7z` must be on `PATH`; used to extract `misc\win32tools.zip`)
- **vcpkg** installed at `C:\vcpkg` with the `x64-windows` triplet, providing the headers/libraries for zlib, zstd, OpenSSL and libsodium:

  ```cmd
  C:\vcpkg\vcpkg install zlib zstd openssl libsodium --triplet x64-windows
  ```

- **Inno Setup 6** (optional; only used to create the installer)

## Clone the Repository

```cmd
git clone https://github.com/soramimi/Guitar.git
cd Guitar
```

## Configure Tool Paths

`scripts\winconf.rb` holds the tool locations used by `make_all.rb`. Edit it if your installation differs:

```ruby
$qt_win = "C:/Qt/6.11.0/msvc2022_64"
$windeployqt = "#{$qt_win}/bin/windeployqt.exe"
$jom_win = "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
$vcvars_bat = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
$innosetup = "C:/Program Files (x86)/Inno Setup 6/ISCC.exe"
```

The vcpkg location (`C:/vcpkg/installed/x64-windows`) is set in `common.pri`.

## Build Everything (Recommended)

```cmd
ruby scripts\make_all.rb
```

This script cleans `_bin`, `lib` and `_build`, runs `prepare.rb`, and then builds, in order:

1. FileTypePlugin (`scripts\win_make_filetypeplugin.bat`)
2. IncrementalSearchPlugin (`scripts\win_make_incrementalsearchplugin.bat`)
3. Guitar (`scripts\win_make_guitar_app.bat`), which also extracts `misc\win32tools.zip` (helper tools and runtime DLLs such as libsodium, OpenSSL, zlib and zstd) into `_bin` and runs `windeployqt`

Each batch file calls `vcvars64.bat` by itself, so a Developer Command Prompt is not required.

Only the release configuration is built by default. To also build the debug configuration, set `$make_debug = true` in `scripts\make_all.rb`.

Finally `make_all.rb` creates the installer with Inno Setup (`_release\Setup-Guitar-*.exe`). Note that the final step also attempts to upload the installer to the server configured in `scripts\deployconf.rb`. If you only want a local build, you can ignore the upload failure, or run the batch files individually as described below.

## Build Manually

The batch files read these optional environment variables (defaults point to the paths above):

- `QMAKE`, `JOM`, `VCVARS_BAT`, `WINDEPLOYQT`
- `NO_DEBUG=1` : skip the debug build
- `NO_RELEASE=1` : skip the release build

Run `ruby prepare.rb` first, then, from the repository root (using `NO_DEBUG=1` to build release only):

```cmd
ruby prepare.rb
set NO_DEBUG=1
scripts\win_make_filetypeplugin.bat
scripts\win_make_incrementalsearchplugin.bat
scripts\win_make_guitar_app.bat
```

## Running Guitar

```cmd
_bin\Guitar.exe
```

(The debug build is `_bin\Guitard.exe`.)
