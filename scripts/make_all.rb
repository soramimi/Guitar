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

require 'fileutils'

# Resolve key directory paths relative to this script.
$script_dir = __dir__
$project_dir = File.realpath("#{$script_dir}/..")
$bin_dir = $project_dir + "/_bin"
$lib_dir = $project_dir + "/lib"
$release_dir = "#{$project_dir}/_release"

$deploy = ENV["DEPLOY"] || false
$suffix = ENV["BRANCH_NAME"] || ""
if $suffix == "master" or $suffix == "main" then
	$suffix = ""
end

# Run the project's preparation script (code generation, etc.) before building.
load "#{$project_dir}/prepare.rb"
$branch_name = `git rev-parse --abbrev-ref HEAD`.strip

# Platform detection flags.
$is_win = false  # true when building with MSVC on Windows
$is_gcc = false  # true when building with GCC on Linux

# Build configuration flags.
$make_debug = false    # set to true to also build the debug configuration
$make_release = true   # set to false to skip the release configuration

# Deployment configuration.
load "#{$script_dir}/deployconf.rb"

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
	load "#{$script_dir}/winconf.rb"
when "linux"
	$is_gcc = true
when "darwin"
	puts "MacOS is no longer supported."
	exit 1
else
	puts "Unknown platform: #{RUBY_PLATFORM}"
	exit 1
end

$win_installer_name = "Setup-Guitar-#{$version_a}.#{$version_b}.#{$version_c}#{$suffix}-windows-x64"
$win_installer_exe = "#{$win_installer_name}.exe"

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

def make_installer()
	if $is_win then
		run "\"#{$innosetup}\" #{$project_dir}/scripts/Guitar.iss /DProjectRoot=\"#{$project_dir}\" /O\"#{$release_dir}\" /F\"#{$win_installer_name}\""
	end
	if $is_gcc then
		puts "Linux installer not implemented yet."
	end
end

def deploy()
	if $is_win then
		run "curl -T #{$release_dir}/#{$win_installer_exe} ftp://#{$destination_path}/#{$win_installer_exe}"
	end
	if $is_gcc then
		puts "Linux deployment not implemented yet."
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
	make_installer()
	if $deploy != nil then
		deploy()
	end
end

main()
