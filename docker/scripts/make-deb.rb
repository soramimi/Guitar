#!/usr/bin/ruby

require 'fileutils'

load '/Guitar/version.rb'

$suffix = ENV['SUFFIX']
if $suffix.nil?
	$suffix = ''
end

$package = "guitar" + $suffix
$maintainer = "nobody <nobody@example.com>"
$version = "#{$version_a}.#{$version_b}.#{$version_c}"
$workdir = "_build"
$bindir = "_build"
$dstdir = $workdir + "/#{$package}"

$arch = `/Guitar/packaging/deb/arch.rb`.strip

FileUtils.mkpath($dstdir + "/DEBIAN")
FileUtils.mkpath($dstdir + "/usr/bin")
FileUtils.mkpath($dstdir + "/usr/share/applications")
FileUtils.mkpath($dstdir + "/usr/share/icons/guitar")
system "strip Guitar"
FileUtils.cp("./_build/Guitar", $dstdir + "/usr/bin/")
FileUtils.cp("/Guitar/LinuxDesktop/Guitar.svg", $dstdir + "/usr/share/icons/guitar/")
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
Depends: libqt6widgets6 (>= 6.4.2), libqt6xml6t64 (>= 6.4.2), libqt6svg6 (>= 6.4.2), zlib1g, git, libmagic1, libmagic-mgc, desktop-file-utils
Description: Git GUI Client
___
}

FileUtils.cp("/Guitar/packaging/deb/postinst", $dstdir + "/DEBIAN/")

system "fakeroot dpkg-deb --build #{$workdir}/#{$package} ."
