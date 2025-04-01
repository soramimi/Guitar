# Building Guitar on Ubuntu

This guide provides step-by-step instructions for building Guitar, a cross-platform Git GUI client, on Ubuntu and other Debian-based Linux distributions.

## Prerequisites

Before you begin, you'll need to install the required dependencies:

```bash
sudo apt install build-essential ruby qmake6 libqt6core6 libqt6gui6 libqt6svg6-dev libqt6core5compat6-dev zlib1g-dev libgl1-mesa-dev libssl-dev
```

These packages include:
- C++ build tools
- Ruby (required for the preparation script)
- Qt6 development libraries
- Other necessary dependencies

## Building Process

### 1. Clone the Repository

First, clone the Guitar repository:

```bash
git clone https://github.com/soramimi/Guitar.git
cd Guitar
```

### 2. Run the Preparation Script

The preparation script sets up the build environment:

```bash
ruby prepare.rb
```

This script performs necessary setup tasks for the build process.

### 3. Configure and Build

Create a build directory and compile the project:

```bash
mkdir build
cd build
qmake6 ../Guitar.pro
make -j8
```

The `-j8` flag enables parallel compilation using 8 threads. You can adjust this number based on your CPU cores.

### 4. Running Guitar

After successful compilation, you can run Guitar from the build directory:

```bash
../_bin/Guitar
```

