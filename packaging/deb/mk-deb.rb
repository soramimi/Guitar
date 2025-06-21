#!/usr/bin/ruby

require 'fileutils'

load '../../version.rb'

$suffix = ENV['SUFFIX']
if $suffix.nil?
	$suffix = ''
end

$package = "guitar" + $suffix
$maintainer = "S.Fuchita <fi7s-fct@asahi-net.or.jp>"
$version = "#{$version_a}.#{$version_b}.#{$version_c}"
$workdir = "build"
$bindir = "build"
$dstdir = $workdir + "/#{$package}"

$arch = `./arch.rb`.strip

FileUtils.mkpath($dstdir + "/DEBIAN")
FileUtils.mkpath($dstdir + "/usr/bin")
FileUtils.mkpath($dstdir + "/usr/share/applications")
FileUtils.mkpath($dstdir + "/usr/share/icons/guitar")
system "strip Guitar"
FileUtils.cp("Guitar", $dstdir + "/usr/bin/")
FileUtils.cp("../../LinuxDesktop/Guitar.svg", $dstdir + "/usr/share/icons/guitar/")
File.open($dstdir + "/usr/share/applications/Guitar.desktop", "w") {|f|
	f.puts <<___
[Desktop Entry]
Type=Application
Name=Guitar
Categories=Development
Exec=/usr/bin/Guitar
Icon=/usr/share/icons/guitar/Guitar.svg
Terminal=false
___
}

File.open($dstdir + "/DEBIAN/control", "w") {|f|
	f.puts <<___
Package: #{$package}
Section: vcs
Maintainer: #{$maintainer}
Architecture: #{$arch}
Version: #{$version}
Depends: libqt6widgets6 (>= 6.2.4), libqt6xml6 (>= 6.2.4), ilibqt6svg6 (>= 6.2.4), zlib1g
Description: Git GUI Client
___
}

FileUtils.cp("postinst", $dstdir + "/DEBIAN/")

system "fakeroot dpkg-deb --build #{$workdir}/#{$package} ."
