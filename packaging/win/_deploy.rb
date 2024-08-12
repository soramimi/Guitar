
require 'fileutils'
$script_dir = __dir__

$qtver = "6.7.2"

if $suffix == nil then
	$suffix = ""
else
	$suffix = '-' + $suffix
end

FileUtils.rm_rf $script_dir + "/build"
FileUtils.rm_rf $script_dir + "/packages/jp.soramimi.guitar/data"

def mkcd(dir)
	FileUtils.mkpath dir
	FileUtils.chdir dir
end

def run(cmd)
	r = system cmd
	if !r then
		abort "FATAL: " + cmd
	end
end

Dir.chdir $script_dir
FileUtils.rm_rf "zlib"
run "git clone https://github.com/madler/zlib"
FileUtils.cp "../../zlib.pro", "zlib/"
Dir.chdir("zlib") {
	run "C:/Qt/#{$qtver}/msvc2019_64/bin/qmake.exe CONFIG+=release zlib.pro"
	run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
}
ENV["INCLUDE"] = $script_dir + "/zlib;" + ENV["INCLUDE"]

Dir.chdir $script_dir + "/../../"
FileUtils.mkpath "_bin"
run "ruby prepare.rb"

FileUtils.cp $script_dir + "/zlib/_bin/libz.lib", "_bin/"

mkcd $script_dir + "/build"
run "C:/Qt/#{$qtver}/msvc2019_64/bin/qmake.exe CONFIG+=release ../../../Guitar.pro"
run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"

Dir.chdir $script_dir + "/../../"
run "ruby RELEASE-WINDOWS.rb"

load 'version.rb'

srcname = "Guitar-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip"
dstname = "Guitar#{$suffix}-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip"

run "curl -T _release/#{srcname} ftp://192.168.0.5:/Public/pub/nightlybuild/#{dstname}"
