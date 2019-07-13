#!/usr/bin/ruby

load '../../version.rb'

$package = "guitar"
$arch = `./arch.rb`.strip
$debname = "#{$package}_#{$version_a}.#{$version_b}.#{$version_c}_#{$arch}.deb"

puts $debname

