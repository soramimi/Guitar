#!/usr/bin/ruby

$qt = "/opt/Qt5.8.0"

require 'fileutils'

load 'version.rb'

$workdir = "_release"
$dstdir = $workdir + "/#{$product_name}"

$dstdir_iconengines = $dstdir + "/iconengines"
$dstdir_imageformats = $dstdir + "/imageformats"
$dstdir_platforms = $dstdir + "/platforms"
$dstdir_platforminputcontexts = $dstdir + "/platforminputcontexts"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)
FileUtils.mkpath($dstdir_iconengines)
FileUtils.mkpath($dstdir_imageformats)
FileUtils.mkpath($dstdir_platforms)
FileUtils.mkpath($dstdir_platforminputcontexts)

FileUtils.cp("../_build_#{$product_name}_Release/#{$product_name}", $dstdir)


def cp_qt_lib(name)
	src = $qt + "/lib/lib" + name + ".so.5"
	FileUtils.cp(src, $dstdir)
end

cp_qt_lib("Qt5Core")
cp_qt_lib("Qt5Gui")
cp_qt_lib("Qt5Svg")
cp_qt_lib("Qt5Widgets")
cp_qt_lib("Qt5Xml")

def cp_qt_imageformat(name)
	src = $qt + "/plugins/imageformats/lib" + name + ".so"
	FileUtils.cp(src, $dstdir_imageformats)
end

cp_qt_imageformat("qgif")
cp_qt_imageformat("qicns")
cp_qt_imageformat("qico")
cp_qt_imageformat("qjpeg")
cp_qt_imageformat("qsvg")

def cp_qt_iconengine(name)
	src = $qt + "/plugins/iconengines/lib" + name + ".so"
	FileUtils.cp(src, $dstdir_iconengines)
end

cp_qt_iconengine("qsvgicon")

src = $qt + "/plugins/platforminputcontexts/libibusplatforminputcontextplugin.so"
FileUtils.cp(src, $dstdir_platforminputcontexts)

$arch = "x86-32bit"
if `uname -a` =~ /(x86_64)|(amd64)/
	$arch = "x86-64bit"
elsif `uname -a` =~ /armv7l/
	$arch = "raspi"
end

Dir.chdir($workdir) {
	`tar zcvf #{$product_name}-#{$version_a}.#{$version_b}.#{$version_c}-linux-#{$arch}.tar.gz #{$product_name}`
}

