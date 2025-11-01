require 'fileutils'

load __dir__ + "/config.rb"
load "#{$project_root}/version.rb"

innosetup = "C:/Program Files (x86)/Inno Setup 6/ISCC.exe"
destdir = "#{$project_root}/_release"
filename = "Setup-Guitar-#{$version_a}.#{$version_b}.#{$version_c}#{$suffix}-windows-x64"
filename_exe = "#{filename}.exe"
system "\"#{innosetup}\" #{$project_root}/packaging/win/InnoSetup/Guitar.iss /DProjectRoot=\"#{$project_root}\" /O\"#{destdir}\" /F\"#{filename}\""
system "curl -T #{destdir}/#{filename_exe} ftp://#{$destination_path}/#{filename_exe}"

