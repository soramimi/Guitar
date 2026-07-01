#!/usr/bin/ruby
# Master build script for the Guitar project.
# Detects the current platform (Windows/Linux) and builds all components:
#   1. FileTypePlugin subproject
#   2. IncrementalSearchPlugin subproject
#   3. Guitar main application
# MacOS is no longer supported.
#
# Usage: ruby make_all.rb
#
# Windows path constants below can be adjusted to match your local Qt installation.

# Resolve key directory paths relative to this script.
$script_dir = __dir__
$project_dir = File.realpath("#{$script_dir}/..")
$bin_dir = $project_dir + "/_bin"
$lib_dir = $project_dir + "/lib"

# Platform detection flags.
$is_win = false  # true when building with MSVC on Windows
$is_gcc = false  # true when building with GCC on Linux

# Build configuration flags.
$make_debug = false    # set to true to also build the debug configuration
$make_release = true   # set to false to skip the release configuration

# Windows-specific tool paths (MSVC / Qt installation locations).
$qt_win = ""
$jom_win = ""
$vcvars_bat = ""
$windeployqt = ""

# Detect the current platform and configure tool paths accordingly.
RUBY_PLATFORM =~ /(mswin|mingw|cygwin|linux|darwin)/
$platform = $1
case $platform
when "mswin", "mingw", "cygwin"
	$is_win = true
	# Adjust these paths to match your local Qt and Visual Studio installation.
	$qt_win = "C:/Qt/6.11.0/msvc2022_64"
	$windeployqt = $qt_win + "/bin/windeployqt.exe"
	$jom_win = "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
	$vcvars_bat = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
when "linux"
	$is_gcc = true
when "darwin"
	puts "MacOS is no longer supported."
	exit 1
else
	puts "Unknown platform: #{RUBY_PLATFORM}"
	exit 1
end

require 'fileutils'

# Run the project's preparation script (code generation, etc.) before building.
load "#{$project_dir}/prepare.rb"

# Set environment variables used by the platform-specific build scripts.
if $is_win
	# Pass Windows tool paths to the batch scripts as environment variables.
	ENV["BINDIR"] = ($bin_dir).gsub("/", "\\")
	ENV["LIBDIR"] = ($lib_dir).gsub("/", "\\")
	ENV["QMAKE"] = ($qt_win + "/bin/qmake.exe").gsub("/", "\\")
	ENV["JOM"] = ($jom_win).gsub("/", "\\")
	ENV["VCVARS_BAT"] = ($vcvars_bat).gsub("/", "\\")
	ENV["WINDEPLOYQT"] = ($windeployqt).gsub("/", "\\")
end
# Propagate build configuration flags to child scripts.
if not $make_debug then
	ENV["NO_DEBUG"] = "1"
end
if not $make_release then
	ENV["NO_RELEASE"] = "1"
end

# Run a shell command and abort the entire build if it fails.
def run(cmd)
	printf "--------\n> %s\n--------\n", cmd
	r = system cmd
	if !r then
		abort "FATAL: " + cmd
	end
end

# Run a build script from the scripts directory.
def run_script(command)
	Dir.chdir($script_dir) {
		run command
	}
end

# Build a subproject in-place using its own GCC build script (Linux only).
def make_subproject_with_gcc(subproject_name)
	Dir.chdir($project_dir + "/subprojects/#{subproject_name}") {
		run "bash build-gcc.sh"
	}
end

# Build the FileTypePlugin subproject for the current platform.
def make_filetypeplugin()
	if $is_win then
		run_script("win_make_filetypeplugin.bat")
	end
	if $is_gcc then
		make_subproject_with_gcc("FileTypePlugin")
	end
end

# Build the IncrementalSearchPlugin subproject for the current platform.
def make_incrementalsearchplugin()
	if $is_win then
		run_script("win_make_incrementalsearchplugin.bat")
	end
	if $is_gcc then
		make_subproject_with_gcc("IncrementalSearchPlugin")
	end
end

# Build the main Guitar application for the current platform.
def make_guitar_app()
	if $is_win then
		run_script("win_make_guitar_app.bat")
	end
	if $is_gcc then
		run_script("bash gcc_make_guitar_app.sh")
	end
end

# Remove all build output directories to start fresh.
def clean()
	FileUtils.rm_rf($bin_dir)
	FileUtils.rm_rf($lib_dir)
	FileUtils.rm_rf($project_dir + "/_build")
end

# Entry point: clean, then build all components in dependency order.
def main()
	clean()
	make_filetypeplugin()
	make_incrementalsearchplugin()
	make_guitar_app()
end

main()
