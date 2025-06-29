#!/usr/bin/ruby

Dir.chdir __dir__
load '../../version.rb'

$suffix = ENV['SUFFIX']
if $suffix.nil?
	$suffix = ''
end

$package = "guitar" + $suffix
$arch = `./arch.rb`.strip
$debname = "#{$package}_#{$version_a}.#{$version_b}.#{$version_c}_#{$arch}.deb"

puts $debname

