# Building Guitar on Ubuntu

This guide explains how to build Guitar on Ubuntu and other Debian-based Linux distributions.

Guitar consists of three projects, all of which must be built:

| Project | Location | Output |
|---|---|---|
| FileTypePlugin (libmagic based file type detection) | `subprojects/FileTypePlugin` | `lib/soramimi.jp/` |
| IncrementalSearchPlugin (MeCab based incremental search) | `subprojects/IncrementalSearchPlugin` | `lib/soramimi.jp/` |
| Guitar (main application) | repository root | `_bin/Guitar` |

## Prerequisites

```bash
sudo apt install \
  build-essential ruby git \
  autoconf automake libtool \
  qmake6 qt6-base-dev qt6-svg-dev libqt6svg6-dev libqt6core5compat6-dev \
  zlib1g-dev libzstd-dev zstd \
  libssl-dev libsodium-dev libcurl4-openssl-dev libsecret-1-dev \
  libgl1-mesa-dev \
  mecab-utils
```

- `ruby` runs the build scripts (`scripts/make_all.rb`, `prepare.rb`).
- `autoconf`, `automake` and `libtool` are used to build libmagic (`file`) and oniguruma bundled with FileTypePlugin.
- `mecab-utils` provides `/usr/lib/mecab/mecab-dict-index`, and `zstd`, `iconv`, `perl` and `gzip` are used to build the dictionary embedded in IncrementalSearchPlugin.

## Clone the Repository

```bash
git clone https://github.com/soramimi/Guitar.git
cd Guitar
```

## Build Everything (Recommended)

If you do not want to set up the build environment yourself, just run:

```bash
ruby scripts/make_all.rb
```

This script cleans `_bin`, `lib` and `_build`, runs `prepare.rb` (generates `version.c` etc.), and then builds, in order:

1. FileTypePlugin (`subprojects/FileTypePlugin/build-gcc.sh`)
2. IncrementalSearchPlugin (`subprojects/IncrementalSearchPlugin/build-gcc.sh`)
3. Guitar (`scripts/gcc_make_guitar_app.sh`)

Only the release configuration is built by default. To also build the debug configuration, set `$make_debug = true` in `scripts/make_all.rb`.

After a successful build, run:

```bash
_bin/Guitar
```

(The debug build is `_bin/Guitard`.) The plugins are placed in `lib/soramimi.jp/`.

## Build Manually

The following is what `make_all.rb` does. Each script honors these environment variables:

- `QMAKE` : qmake executable (default: `qmake6`)
- `NO_DEBUG=1` : skip the debug build
- `NO_RELEASE=1` : skip the release build

Note that the scripts below build both debug and release unless told otherwise. `NO_DEBUG=1` is used in the examples to build release only.

### 1. Run the Preparation Script

```bash
ruby prepare.rb
```

### 2. Build FileTypePlugin

```bash
cd subprojects/FileTypePlugin
NO_DEBUG=1 bash build-gcc.sh
cd ../..
```

This builds libmagic (`file`), oniguruma, and the plugin itself.
An error such as `'sandbox' undeclared` while building `file` is expected and can be ignored. Only the libmagic library is needed, not the `file` command.

### 3. Build IncrementalSearchPlugin

```bash
cd subprojects/IncrementalSearchPlugin
NO_DEBUG=1 bash build-gcc.sh
cd ../..
```

This converts the MeCab dictionary to UTF-8, builds and compresses it, builds the bundled MeCab, and then builds the plugin.

### 4. Build Guitar

```bash
NO_DEBUG=1 bash scripts/gcc_make_guitar_app.sh
```

The build is done in `_build/` and the executable is written to `_bin/`.
