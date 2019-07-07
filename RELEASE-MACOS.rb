#!/usr/bin/ruby

require 'fileutils'

load 'version.rb'

$workdir = "_release"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($workdir)

`cp -a _bin/#{$product_name}.app #{$workdir}/`
#`cp Guitar_ja.qm #{$workdir}/Guitar.app/Contents/Resources`

`/opt/Qt/5.13.0/clang_64/bin/macdeployqt #{$workdir}/#{$product_name}.app`

Dir.chdir($workdir) {
	`zip -r Guitar-#{$version_a}.#{$version_b}.#{$version_c}-macos.zip Guitar.app`
}

