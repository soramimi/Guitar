#!/usr/bin/ruby

$arch = ""
uname_a = `uname -a`
if uname_a =~ /armv7l/
	$arch = "armhf"
elsif uname_a =~ /(arm64)|(aarch64)/
	$arch = "arm64"
elsif uname_a =~ /(x86_64)|(amd64)/
	$arch = "amd64"
else
	$arch = "i386"
end

puts $arch

