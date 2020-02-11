#!/usr/bin/ruby

# create the following files
#	* version.c
#	* win.rc
#	* Info.plist

load 'version.rb'

def get_revision()
	rev = ""
	if Dir.exist?(".git")
		hash = `git rev-parse HEAD`
		if hash =~ /^[0-9A-Za-z]+/
			rev = hash[0, 7]
		end
	end
	return rev
end

File.open("src/version.h", "w") {|f|
	f.puts <<_____
int copyright_year = #{$copyright_year};
char const product_version[] = "#{$version_a}.#{$version_b}.#{$version_c}";
char const source_revision[] = "#{get_revision()}";
_____
}

File.open("win.rc", "w") {|f|
	f.puts <<_____
#include <windows.h>

100 ICON DISCARDABLE "src/resources/#{$product_name}.ico"

VS_VERSION_INFO     VERSIONINFO
 FILEVERSION       #{$version_a},#{$version_b},#{$version_c},#{$version_d}
 PRODUCTVERSION    #{$version_a},#{$version_b},#{$version_c},#{$version_d}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x4L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
	BLOCK "StringFileInfo"
	BEGIN
        BLOCK "041103a4"
        BEGIN
            VALUE "CompanyName", "S.Fuchita"
            VALUE "FileDescription", "The Git GUI Client"
            VALUE "FileVersion", "#{$version_a}, #{$version_b}, #{$version_c}, #{$version_d}"
            VALUE "InternalName",    "#{$product_name}.exe"
            VALUE "LegalCopyright", "Copyright (C) #{$copyright_year} S.Fuchita (@soramimi_jp)"
            VALUE "OriginalFilename","#{$product_name}.exe"
            VALUE "ProductName", "#{$product_name}"
            VALUE "ProductVersion", "#{$version_a}, #{$version_b}, #{$version_c}, #{$version_d}"
        END
	END
	BLOCK "VarFileInfo"
	BEGIN
        VALUE "Translation", 0x0411, 932
    END
END
_____
}

File.open("Info.plist", "w") {|f|
	f.puts <<_____
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>NSPrincipalClass</key>
	<string>NSApplication</string>
	<key>NSHighResolutionCapable</key>
	<string>True</string>
	<key>CFBundleIconFile</key>
	<string>#{$product_name}.icns</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleGetInfoString</key>
	<string>#{$version_a}.#{$version_b}.#{$version_c}</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleExecutable</key>
	<string>#{$product_name}</string>
	<key>CFBundleIdentifier</key>
	<string>jp.soramimi.#{$product_name}</string>
</dict>
</plist>
_____
}
