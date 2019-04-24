#!/usr/bin/ruby

require 'fileutils'

load '../../version.rb'

$package = "guitar"
$maintainer = "foo <foo@example.com>"
$version = "#{$version_a}.#{$version_b}.#{$version_c}"
$workdir = "build"
$bindir = "build"
$dstdir = $workdir + "/#{$package}"

$arch = "i386"
if `uname -a` =~ /(x86_64)|(amd64)/
	$arch = "amd64"
elsif `uname -a` =~ /armv7l/
	$arch = "armhf"
end

FileUtils.mkpath($dstdir + "/DEBIAN")
FileUtils.mkpath($dstdir + "/usr/bin")
FileUtils.mkpath($dstdir + "/usr/share/applications")
FileUtils.mkpath($dstdir + "/usr/share/icons/guitar")
FileUtils.cp("#{$bindir}/Guitar", $dstdir + "/usr/bin/")
system "strip #{$dstdir}/usr/bin/Guitar"
FileUtils.cp("../../LinuxDesktop/Guitar.svg", $dstdir + "/usr/share/icons/guitar/")
File.open($dstdir + "/usr/share/applications/Guitar.desktop", "w") {|f|
	f.puts <<___
[Desktop Entry]
Type=Application
Name=Guitar
Exec=/usr/bin/Guitar
Icon=/usr/share/icons/guitar/Guitar.svg
Terminal=false
___
}

File.open($dstdir + "/DEBIAN/control", "w") {|f|
	f.puts <<___
Package: #{$package}
Maintainer: #{$maintainer}
Architecture: #{$arch}
Version: #{$version}
Depends: libqt5widgets5 (>= 5.5.0), libqt5xml5 (>= 5.5.0), libqt5svg5 (>= 5.5.0), libssl1.0.0, git, file
Description: Git GUI Client
___
}

FileUtils.cp("postinst", $dstdir + "/DEBIAN/")

system "fakeroot dpkg-deb --build #{$workdir}/#{$package} ."
