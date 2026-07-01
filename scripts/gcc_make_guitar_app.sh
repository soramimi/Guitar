#!/bin/bash
# Build script for the main Guitar application using GCC on Linux.
# Builds both debug and release configurations by default.
# Set NO_DEBUG=1 to skip the debug build, or NO_RELEASE=1 to skip the release build.
# The QMAKE environment variable can be used to override the qmake executable
# (defaults to qmake6 if not set).

# Exit immediately on any error, treat unset variables as errors,
# and propagate failures through pipelines.
set -euo pipefail

# Resolve the absolute path to the project root directory.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../" && pwd)"
cd "$DIR"

# Allow overriding the qmake executable via the QMAKE environment variable;
# fall back to qmake6 if not set.
: "${QMAKE:=qmake6}"

# Recreate the build directory to ensure a clean build.
BUILDDIR="$DIR/_build"
rm -fr "$BUILDDIR"
mkdir "$BUILDDIR"
cd "$BUILDDIR"

# Build the debug configuration unless NO_DEBUG is set.
if [ "${NO_DEBUG:=}" = "" ]; then
    $QMAKE  "CONFIG+=debug" "$DIR/Guitar.pro"
    make -j10
fi

# Build the release configuration unless NO_RELEASE is set.
if [ "${NO_RELEASE:=}" = "" ]; then
    $QMAKE  "CONFIG+=release" "$DIR/Guitar.pro"
    make -j10
fi

