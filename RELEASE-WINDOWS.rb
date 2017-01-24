#!/usr/bin/ruby

$qt = "C:/Qt/Qt5.7.1/5.7/msvc2013"

require 'fileutils'

load 'version.rb'

$workdir = "_release"
$dstdir = $workdir + "/Guitar"

$dstdir_iconengines = $dstdir + "/iconengines"
$dstdir_imageformats = $dstdir + "/imageformats"
$dstdir_platforms = $dstdir + "/platforms"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)
FileUtils.mkpath($dstdir_iconengines)
FileUtils.mkpath($dstdir_imageformats)
FileUtils.mkpath($dstdir_platforms)

FileUtils.cp("../_build_Guitar/release/Guitar.exe", $dstdir)

FileUtils.cp("C:/Windows/SysWOW64/msvcp120.dll", $dstdir)
FileUtils.cp("C:/Windows/SysWOW64/msvcr120.dll", $dstdir)


def cp_qt_lib(name)
	src = $qt + "/bin/" + name
	FileUtils.cp(src, $dstdir)
end

cp_qt_lib("Qt5Core.dll")
cp_qt_lib("Qt5Gui.dll")
cp_qt_lib("Qt5Svg.dll")
cp_qt_lib("Qt5Widgets.dll")
cp_qt_lib("Qt5Xml.dll")

def cp_qt_imageformat(name)
	src = $qt + "/plugins/imageformats/" + name
	FileUtils.cp(src, $dstdir_imageformats)
end

cp_qt_imageformat("qgif.dll")
cp_qt_imageformat("qicns.dll")
cp_qt_imageformat("qico.dll")
cp_qt_imageformat("qjpeg.dll")
cp_qt_imageformat("qsvg.dll")

def cp_qt_iconengine(name)
	src = $qt + "/plugins/iconengines/" + name
	FileUtils.cp(src, $dstdir_iconengines)
end

cp_qt_iconengine("qsvgicon.dll")

def cp_qt_platform(name)
	src = $qt + "/plugins/platforms/" + name
	FileUtils.cp(src, $dstdir_platforms)
end

cp_qt_platform("qwindows.dll")
cp_qt_platform("qminimal.dll")
cp_qt_platform("qoffscreen.dll")

Dir.chdir($workdir) {
	`7z a Guitar-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip Guitar`
}

