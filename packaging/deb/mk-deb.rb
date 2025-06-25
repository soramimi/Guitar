#!/usr/bin/ruby

require 'fileutils'

$project_root = '../..'

load "#{$project_root}/version.rb"

$suffix = ENV['SUFFIX']
if $suffix.nil?
  $suffix = ''
end

$package = "guitar" + $suffix
$maintainer = "S.Fuchita <fi7s-fct@asahi-net.or.jp>"
$version = "#{$version_a}.#{$version_b}.#{$version_c}"
$workdir = "build"
$dstdir = $workdir + "/#{$package}"

$arch = `#{$project_root}/packaging/deb/arch.rb`.strip

FileUtils.mkpath($dstdir + "/DEBIAN")
FileUtils.mkpath($dstdir + "/usr/bin")
FileUtils.mkpath($dstdir + "/usr/share/applications")
FileUtils.mkpath($dstdir + "/usr/share/icons/guitar")
FileUtils.cp("#{$project_root}/_bin/Guitar", $dstdir + "/usr/bin/")
system "strip #{$dstdir}/usr/bin/Guitar"
FileUtils.cp("#{$project_root}/LinuxDesktop/Guitar.svg", $dstdir + "/usr/share/icons/guitar/")
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
Depends: libqt6widgets6 (>= 6.2.4), libqt6xml6 (>= 6.2.4), libqt6svg6 (>= 6.2.4), qt6-qpa-plugins (>= 6.2.4), zlib1g, git, desktop-file-utils
Description: Git GUI Client
___
}

FileUtils.cp("#{$project_root}/packaging/deb/postinst", $dstdir + "/DEBIAN/")

system "fakeroot dpkg-deb --build #{$workdir}/#{$package} ."
