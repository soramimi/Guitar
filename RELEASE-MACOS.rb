#!/usr/bin/ruby

require 'fileutils'

load 'version.rb'

$workdir = "_release"

FileUtils.rm_rf($workdir)
FileUtils.mkpath($workdir)

`cp -a ../_build_#{$product_name}_Release/#{$product_name}.app #{$workdir}/`
FileUtils.cp("../_build_AskPass_Release/askpass", "#{$workdir}/#{$product_name}.app/Contents/MacOS/")

`/opt/Qt5.8.0/5.8/clang_64/bin/macdeployqt #{$workdir}/#{$product_name}.app`

Dir.chdir($workdir) {
	`zip -r Guitar-#{$version_a}.#{$version_b}.#{$version_c}-macos.zip Guitar.app`
}

