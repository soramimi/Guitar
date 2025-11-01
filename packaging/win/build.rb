require 'fileutils'

load __dir__ + "/config.rb"

def mkcd(dir)
	FileUtils.rm_rf dir
	FileUtils.mkpath dir
	FileUtils.chdir dir
end

def run(cmd)
	r = system cmd
	if !r then
		abort "FATAL: " + cmd
	end
end

$current_branch = `git symbolic-ref --short HEAD`.strip

$suffix = $current_branch
if $suffix == "main" or $suffix == "master" then
	$suffix = ""
end

FileUtils.rm_rf $script_dir + "/build"
FileUtils.rm_rf $script_dir + "/packages/jp.soramimi.guitar/data"

Dir.chdir $script_dir
FileUtils.rm_rf "zlib"
run "git clone https://github.com/madler/zlib"
FileUtils.cp "../../zlib.pro", "zlib/"
Dir.chdir("zlib") {
	run "C:/Qt/#{$qt}/bin/qmake.exe CONFIG+=release zlib.pro"
	run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
}
ENV["INCLUDE"] = $script_dir + "/zlib;" + ENV["INCLUDE"]

Dir.chdir $script_dir + "/../../"
FileUtils.mkpath "_bin"
run "ruby prepare.rb"

FileUtils.cp $script_dir + "/zlib/_bin/libz.lib", "_bin/"

Dir.chdir("filetype") {
	run "build-msvc.bat"
}

mkcd $script_dir + "/_build_guitar"
run "C:/Qt/#{$qt}/bin/qmake.exe CONFIG+=release ../../../Guitar.pro"
run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe /j 8"

