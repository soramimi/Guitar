
require 'fileutils'
$script_dir = __dir__

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
	run "C:/Qt/6.5.2/msvc2019_64/bin/qmake.exe CONFIG+=release zlib.pro"
	run "C:/Qt/Tools/QtCreator/bin/jom.exe"
}
ENV["INCLUDE"] = $script_dir + "/zlib;" + ENV["INCLUDE"]

Dir.chdir $script_dir + "/../../"
FileUtils.mkpath "_bin"
run "ruby prepare.rb"

FileUtils.cp $script_dir + "/zlib/_bin/libz.lib", "_bin/"

mkcd $script_dir + "/build"
run "C:/Qt/6.5.2/msvc2019_64/bin/qmake.exe CONFIG+=release ../../../Guitar.pro"
run "C:/Qt/Tools/QtCreator/bin/jom.exe"

Dir.chdir $script_dir + "/../../"
run "ruby RELEASE-WINDOWS.rb"

Dir.chdir $script_dir
load '../../version.rb'

pkgname = "Guitar-#{$version_a}.#{$version_b}.#{$version_c}-win32.zip"

run "7z x ../../_release/#{pkgname} -opackages/jp.soramimi.guitar"

mkcd $script_dir + "/packages/jp.soramimi.guitar"
FileUtils.mv "Guitar", "data"

Dir.chdir $script_dir
run "C:/Qt/QtIFW-3.2.2/bin/binarycreator.exe -c config/config.xml -p packages GuitarSetup.exe"

run "curl -T GuitarSetup.exe ftp://10.0.0.5:/Public/pub/nightlybuild/"
