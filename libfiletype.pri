msvc:CONFIG(release, debug|release):LIBS += $$PWD/filetype/lib/filetype.lib $$PWD/filetype/lib/file.lib $$PWD/filetype/lib/oniguruma.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/filetype/lib/filetyped.lib $$PWD/filetype/lib/filed.lib $$PWD/filetype/lib/onigurumad.lib
!msvc:CONFIG(release, debug|release):LIBS += $$PWD/filetype/lib/libfiletype.a $$PWD/filetype/lib/libfile.a $$PWD/filetype/lib/liboniguruma.a
!msvc:CONFIG(debug, debug|release):LIBS += $$PWD/filetype/lib/libfiletyped.a $$PWD/filetype/lib/libfiled.a $$PWD/filetype/lib/libonigurumad.a
