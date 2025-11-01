#!/usr/bin/ruby

$project_dir = "#{__dir__}/.."

require 'fileutils'

load 'version.rb'

$workdir = "#{$project_dir}/_release"

$dstdir = "#{$workdir}/#{$product_name}"

$dstdir_iconengines = $dstdir + "/iconengines"
$dstdir_imageformats = $dstdir + "/imageformats"
$dstdir_platforms = $dstdir + "/platforms"
$dstdir_platforminputcontexts = $dstdir + "/platforminputcontexts"

$arch = "x86-32bit"
$libicu = "/usr/lib/i386-linux-gnu"
if `uname -a` =~ /(x86_64)|(amd64)/
	$arch = "x86-64bit"
	$libicu = "/usr/lib/x86_64-linux-gnu"
elsif `uname -a` =~ /armv7l/
	$arch = "raspberrypi"
	$libicu = "/usr/lib/arm-linux-gnueabihf"
end

FileUtils.rm_rf($workdir)
FileUtils.mkpath($dstdir)

FileUtils.cp("_bin/#{$product_name}", $dstdir)
`strip #{$dstdir}/#{$product_name}`

FileUtils.cp_r("LinuxDesktop", $dstdir)

Dir.chdir($workdir) {
	`tar zcvf #{$product_name}-#{$version_a}.#{$version_b}.#{$version_c}-linux-#{$arch}.tar.gz #{$product_name}`
}

