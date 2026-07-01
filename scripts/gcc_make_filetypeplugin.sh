#! /bin/bash
# Build script for the FileTypePlugin subproject using GCC on Linux.
# Resolves the absolute path to the FileTypePlugin directory relative to this
# script's location, then delegates to the subproject's own build-gcc.sh.

# Exit immediately on any error, treat unset variables as errors,
# and propagate failures through pipelines.
set -euo pipefail

# Resolve the absolute path to the FileTypePlugin subproject directory.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../subprojects/FileTypePlugin" && pwd)"
cd "$DIR"

# Delegate to the subproject's GCC build script.
bash build-gcc.sh
