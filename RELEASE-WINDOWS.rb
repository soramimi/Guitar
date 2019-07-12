#!/usr/bin/ruby

$qt = "C:/Qt/Qt5.7.1/5.7/msvc2013"
#$qt = "" # スタティックリンクのとき

$openssl = "C:/openssl"

require 'fileutils'

load 'version.rb'

$workdir = "_release"
$dstdir = $workdir + "/" + $product_name

$dstdir_iconengines = $dstdir + "/iconengines"
$dstdir_imageformats = $dstdir + "/imageformats"
$dstdir_platforms = $dstdir + "/platforms"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)

FileUtils.cp("_bin/#{$product_name}.exe", $dstdir)
#FileUtils.cp("src/resources/translations/Guitar_ja.qm", $dstdir)

FileUtils.cp($openssl + "/bin/libeay32.dll", $dstdir)
FileUtils.cp($openssl + "/bin/ssleay32.dll", $dstdir)

`7z x -o#{$dstdir} misc/win32tools.zip`
`move #{$dstdir}\\win32tools\\* #{$dstdir}`
FileUtils.rmdir("#{$dstdir}\\win32tools")

if $qt != ''

	FileUtils.mkpath($dstdir_iconengines)
	FileUtils.mkpath($dstdir_imageformats)
	FileUtils.mkpath($dstdir_platforms)

	def cp_qt_lib(name)
		src = $qt + "/bin/" + name
		FileUtils.cp(src, $dstdir)
	end

	cp_qt_lib("Qt5Core.dll")
	cp_qt_lib("Qt5Gui.dll")
	cp_qt_lib("Qt5Svg.dll")
	cp_qt_lib("Qt5Widgets.dll")
	cp_qt_lib("Qt5Xml.dll")
	cp_qt_lib("Qt5Network.dll")
	cp_qt_lib("Qt5WinExtras.dll")

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

end

pkgfilename = "#{$product_name}-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip"

Dir.chdir($workdir) {
	`7z a #{pkgfilename} #{$product_name}`
}

Dir.chdir("packaging/win") {
	`mk.bat #{pkgfilename}`
}

