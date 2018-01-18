#!/usr/bin/ruby

require 'fileutils'

load 'version.rb'

$workdir = "_release"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($workdir)

`cp -a ../_build_#{$product_name}_Release/#{$product_name}.app #{$workdir}/`

`/opt/Qt5.9.2/5.9.2/clang_64/bin/macdeployqt #{$workdir}/#{$product_name}.app`

Dir.chdir($workdir) {
	`zip -r Guitar-#{$version_a}.#{$version_b}.#{$version_c}-macos.zip Guitar.app`
}

