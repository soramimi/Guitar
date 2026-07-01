#!/usr/bin/ruby

$script_dir = __dir__
$project_dir = File.realpath("#{$script_dir}/..")
$bin_dir = $project_dir + "/_bin"
$lib_dir = $project_dir + "/lib"

$is_msc = false
$is_gcc = false
$make_debug = false
$make_release = true
$qt_win = ""
$jom_win = ""
$vcvars_bat = ""
$windeployqt = ""

RUBY_PLATFORM =~ /(mswin|mingw|cygwin|linux|darwin)/
$platform = $1
case $platform
when "mswin", "mingw", "cygwin"
	$is_msc = true
	$qt_win = "C:/Qt/6.11.0/msvc2022_64"
	$jom_win = "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
	$vcvars_bat = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
	$windeployqt = $qt_win + "/bin/windeployqt.exe"
when "linux"
	$is_gcc = true
end

p $qt_win
p $platform
p $script_dir
p $project_dir

require 'fileutils'

load "#{$project_dir}/prepare.rb"

def run(cmd)
	printf "--------\n> %s\n--------\n", cmd
	r = system cmd
	if !r then
		abort "FATAL: " + cmd
	end
end
	
def make_for_windows(batfile)
	Dir.chdir($project_dir + "/misc") {
		ENV["BINDIR"] = ($bin_dir).gsub("/", "\\")
		ENV["LIBDIR"] = ($lib_dir).gsub("/", "\\")
		ENV["QMAKE"] = ($qt_win + "/bin/qmake.exe").gsub("/", "\\")
		ENV["JOM"] = ($jom_win).gsub("/", "\\")
		ENV["VCVARS_BAT"] = ($vcvars_bat).gsub("/", "\\")
		ENV["WINDEPLOYQT"] = ($windeployqt).gsub("/", "\\")
		if not $make_debug then
			ENV["NO_DEBUG"] = "1"
		end
		if not $make_release then
			ENV["NO_RELEASE"] = "1"
		end
		run batfile
	}
end

def make_filetypeplugin()
	if $is_msc then
		make_for_windows("win_make_filetypeplugin.bat")
	end
end

def make_incrementalsearchplugin()
	if $is_msc then
		make_for_windows("win_make_incrementalsearchplugin.bat")
	end
end

def make_guitar_app()
	if $is_msc then
		make_for_windows("win_make_guitar.bat")
	end
end

def clean()
	FileUtils.rm_rf($bin_dir)
	FileUtils.rm_rf($lib_dir)
	FileUtils.rm_rf($project_dir + "/_build")
end

def main()
	clean()
	make_filetypeplugin()
	make_incrementalsearchplugin()
	make_guitar_app()
end

main()
