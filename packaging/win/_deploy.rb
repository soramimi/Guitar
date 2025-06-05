require 'fileutils'
$script_dir = __dir__
$project_root = Dir.pwd

$qtver = "6.9.0"

if $suffix == nil then
	$suffix = ""
end

FileUtils.rm_rf $script_dir + "/build"
FileUtils.rm_rf $script_dir + "/packages/jp.soramimi.guitar/data"

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

Dir.chdir $script_dir
FileUtils.rm_rf "zlib"
run "git clone https://github.com/madler/zlib"
FileUtils.cp "../../zlib.pro", "zlib/"
Dir.chdir("zlib") {
	run "C:/Qt/#{$qtver}/msvc2022_64/bin/qmake.exe CONFIG+=release zlib.pro"
	run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
}
ENV["INCLUDE"] = $script_dir + "/zlib;" + ENV["INCLUDE"]

Dir.chdir $script_dir + "/../../"
FileUtils.mkpath "_bin"
run "ruby prepare.rb"

FileUtils.cp $script_dir + "/zlib/_bin/libz.lib", "_bin/"

Dir.chdir("filetype") {
	mkcd $script_dir + "/_build_filetype"
	run "C:/Qt/#{$qtver}/msvc2022_64/bin/qmake.exe CONFIG+=release ../../../filetype/libfiletype.pro"
	run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"
}

mkcd $script_dir + "/_build_guitar"
run "C:/Qt/#{$qtver}/msvc2022_64/bin/qmake.exe CONFIG+=release ../../../Guitar.pro"
run "C:/Qt/Tools/QtCreator/bin/jom/jom.exe"

Dir.chdir $script_dir + "/../../"
run "ruby RELEASE-WINDOWS.rb"

load 'version.rb'

srcname = "Guitar-#{$version_a}.#{$version_b}.#{$version_c}-windows-x64.zip"
dstname = "Guitar-#{$version_a}.#{$version_b}.#{$version_c}#{$suffix}-windows-x64.zip"
run "curl -T _release/#{srcname} ftp://192.168.0.5:/Public/pub/nightlybuild/#{dstname}"

innosetup = "C:/Program Files (x86)/Inno Setup 6/ISCC.exe"
destdir = "#{$project_root}/_release"
filename = "Setup-Guitar-#{$version_a}.#{$version_b}.#{$version_c}#{$suffix}-windows-x64"
filename_exe = "#{filename}.exe"
system("\"#{innosetup}\" packaging/win/InnoSetup/Guitar.iss /DProjectRoot=\"#{$project_root}\" /O\"#{destdir}\" /F\"#{filename}\"")
run "curl -T #{destdir}/#{filename_exe} ftp://192.168.0.5:/Public/pub/nightlybuild/#{filename_exe}"
