#!/usr/bin/ruby

require 'fileutils'

$script_dir = __dir__
$project_dir = File.realpath("#{$script_dir}/..")

Dir.chdir($script_dir)

$project_root = '../..'

load "#{$project_root}/version.rb"

$suffix = ENV['SUFFIX']
if $suffix.nil?
  $suffix = ''
end

$package = "guitar"
$maintainer = "S.Fuchita <fi7s-fct@asahi-net.or.jp>"
$version = "#{$version_a}.#{$version_b}.#{$version_c}" + $suffix
$workdir = "build"
$dstdir = $workdir + "/#{$package}"

$arch = IO.popen([RbConfig.ruby, File.join($script_dir, "arch.rb")], &:read).strip
abort "Could not determine package architecture" unless $?.success? && !$arch.empty?

FileUtils.rm_rf($workdir)
File.umask(0o022)
FileUtils.mkpath($dstdir + "/DEBIAN")
FileUtils.mkpath($dstdir + "/usr/bin")
FileUtils.mkpath($dstdir + "/usr/lib/soramimi.jp")
FileUtils.mkpath($dstdir + "/usr/share/applications")
FileUtils.mkpath($dstdir + "/usr/share/icons/guitar")
FileUtils.install("#{$project_root}/_bin/Guitar", $dstdir + "/usr/bin/", mode: 0o755)
FileUtils.install("#{$project_root}/lib/soramimi.jp/libfiletypeplugin.so", $dstdir + "/usr/lib/soramimi.jp/", mode: 0o644)
FileUtils.install("#{$project_root}/lib/soramimi.jp/libincrementalsearchplugin.so", $dstdir + "/usr/lib/soramimi.jp/", mode: 0o644)
system "strip #{$dstdir}/usr/bin/Guitar"
FileUtils.install("#{$project_root}/LinuxDesktop/Guitar.svg", $dstdir + "/usr/share/icons/guitar/", mode: 0o644)
File.open($dstdir + "/usr/share/applications/Guitar.desktop", "w") {|f|
	f.puts <<___
[Desktop Entry]
Type=Application
Name=Guitar
Categories=Development;RevisionControl;
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
Depends: libqt6widgets6 (>= 6.2.4), libqt6xml6 (>= 6.2.4), libqt6svg6 (>= 6.2.4), qt6-qpa-plugins (>= 6.2.4), libsecret-1-0, libsodium23, libssl3t64 | libssl3, zlib1g, zstd, git, desktop-file-utils
Description: Git GUI Client
___
}

FileUtils.install("#{$project_root}/packaging/deb/postinst", $dstdir + "/DEBIAN/", mode: 0o755)

abort "Could not build Debian package" unless system("dpkg-deb", "--root-owner-group", "--build", $dstdir, ".")
