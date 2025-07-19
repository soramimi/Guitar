#!/usr/bin/ruby

Dir.chdir __dir__
load '../../version.rb'

$suffix = ENV['SUFFIX']
if $suffix.nil?
	$suffix = ''
end

$package = "guitar"
$arch = `./arch.rb`.strip
$debname = "#{$package}_#{$version_a}.#{$version_b}.#{$version_c}#{$suffix}_#{$arch}.deb"

puts $debname

