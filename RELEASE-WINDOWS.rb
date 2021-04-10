#!/usr/bin/ruby

$qt = "C:/Qt/5.15.0/msvc2019"
$windeployqt = "C:/Qt/5.15.0/msvc2019/bin/windeployqt.exe"
#$qt = "" # スタティックリンクのとき

$openssl = "C:/Program Files (x86)/OpenSSL"

require 'fileutils'

load 'version.rb'

$workdir = "_release"
$dstdir = $workdir + "/" + $product_name

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)

FileUtils.cp("_bin/#{$product_name}.exe", $dstdir)
#FileUtils.cp("src/resources/translations/Guitar_ja.qm", $dstdir)

FileUtils.cp($openssl + "/bin/libssl-1_1.dll", $dstdir)
FileUtils.cp($openssl + "/bin/libcrypto-1_1.dll", $dstdir)

`7z x -o#{$dstdir} misc/win32tools.zip`
`move #{$dstdir}\\win32tools\\* #{$dstdir}`
FileUtils.rmdir("#{$dstdir}\\win32tools")

`#{$windeployqt} #{$dstdir}/#{$product_name}.exe`

pkgfilename = "#{$product_name}-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip"

Dir.chdir($workdir) {
	`7z a #{pkgfilename} #{$product_name}`
}

