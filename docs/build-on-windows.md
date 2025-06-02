# Building Guitar on Windows

This guide provides step-by-step instructions for building Guitar, a cross-platform Git GUI client, on Windows using Visual Studio and Qt.

## Prerequisites

Before you begin, you'll need to install the following software:

- **Visual Studio** with C++ development tools
  - Visual Studio 2022 Community Edition is recommended
- **Qt 6.9.0** or later for Windows/MSVC
  - Available from the [Qt website](https://www.qt.io/download)
- **Ruby for Windows**
  - Required for the preparation script
- **Git for Windows**
  - For cloning the repository
- **7-Zip**
  - For extracting Windows tools archive

## Building Process

### 1. Set Up the Development Environment

Open a command prompt with Visual Studio environment variables:

```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### 2. Clone the Repository

Clone the Guitar repository:

```cmd
git clone https://github.com/soramimi/Guitar.git
cd Guitar
```

### 3. Run the Preparation Script

The preparation script sets up the build environment:

```cmd
ruby prepare.rb
```

### 4. Build the zlib Dependency

Guitar requires zlib to be built first:

```cmd
rmdir /s /q build
mkdir build
cd build
c:\Qt\6.9.0\msvc2022_64\bin\qmake.exe ..\zlib.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..
```

Note: Adjust the Qt path according to your installation.

### 5. Build the libfiletype Dependency

Guitar requires libfiletype to be built second:

```cmd
cd filetype
rmdir /s /q build
mkdir build
cd build
c:\Qt\6.9.0\msvc2022_64\bin\qmake.exe ..\libfiletype.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..\..
```

### 6. Build Guitar

Now build the main application:

```cmd
rmdir /s /q build
mkdir build
cd build
c:\Qt\6.9.0\msvc2022_64\bin\qmake.exe ../Guitar.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
cd ..
```

### 7. Prepare the Executable

Finally, set up the executable with required dependencies:

```cmd
cd _bin
del libz.lib
7z e ..\misc\win32tools.zip
C:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe Guitar.exe
```

This uses the Qt deployment tool to copy all required Qt libraries to the application folder.

### 8. Running Guitar

After successful completion, you can run Guitar from the `_bin` directory:

```cmd
Guitar.exe
```