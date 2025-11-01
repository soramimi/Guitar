#!/usr/bin/ruby

load __dir__ + "/config.rb"

$windeployqt = "C:/Qt/#{$qt}/bin/windeployqt.exe"

require 'fileutils'

load 'version.rb'

$workdir = $project_root + "/_release"
$dstdir = $workdir + "/soramimi.jp/" + $product_name

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)

FileUtils.cp("_bin/#{$product_name}.exe", $dstdir)

`7z x -o#{$dstdir} misc/win32tools.zip`
`move #{$dstdir}\\win32tools\\* #{$dstdir}`
FileUtils.rmdir("#{$dstdir}\\win32tools")

`#{$windeployqt} #{$dstdir}/#{$product_name}.exe`

pkgfilename = "#{$product_name}-#{$version_a}.#{$version_b}.#{$version_c}-windows-x64.zip"

Dir.chdir($workdir) {
	`7z a #{pkgfilename} soramimi.jp`
}

