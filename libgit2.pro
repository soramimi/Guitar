TARGET = libgit2
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++11 console
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += $$PWD/../libgit2/src
INCLUDEPATH += $$PWD/../libgit2/include
INCLUDEPATH += $$PWD/../libgit2/deps/regex
INCLUDEPATH += $$PWD/../libgit2/deps/zlib
INCLUDEPATH += $$PWD/../libgit2/deps/http-parser

QMAKE_CFLAGS += -DGIT_THREADS

win32:QMAKE_CFLAGS += -Dst_ctime_nsec=st_ctim.tv_nsec
win32:QMAKE_CFLAGS += -Dst_mtime_nsec=st_mtim.tv_nsec
linux:QMAKE_CFLAGS += -Dst_ctime_nsec=st_ctim.tv_nsec
linux:QMAKE_CFLAGS += -Dst_mtime_nsec=st_mtim.tv_nsec
macx:QMAKE_CFLAGS += -Dst_ctime_nsec=st_ctimespec.tv_nsec -DSTDC
macx:QMAKE_CFLAGS += -Dst_mtime_nsec=st_mtimespec.tv_nsec -DSTDC

HEADERS += \
    ../libgit2/src/annotated_commit.h \
    ../libgit2/src/apply.h \
    ../libgit2/src/array.h \
    ../libgit2/src/attr.h \
    ../libgit2/src/attr_file.h \
    ../libgit2/src/attrcache.h \
    ../libgit2/src/bitvec.h \
    ../libgit2/src/blame.h \
    ../libgit2/src/blame_git.h \
    ../libgit2/src/blob.h \
    ../libgit2/src/branch.h \
    ../libgit2/src/buf_text.h \
    ../libgit2/src/buffer.h \
    ../libgit2/src/cache.h \
    ../libgit2/src/cc-compat.h \
    ../libgit2/src/checkout.h \
    ../libgit2/src/clone.h \
    ../libgit2/src/commit.h \
    ../libgit2/src/commit_list.h \
    ../libgit2/src/common.h \
    ../libgit2/src/config.h \
    ../libgit2/src/config_file.h \
    ../libgit2/src/curl_stream.h \
    ../libgit2/src/delta.h \
    ../libgit2/src/diff.h \
    ../libgit2/src/diff_driver.h \
    ../libgit2/src/diff_file.h \
    ../libgit2/src/diff_generate.h \
    ../libgit2/src/diff_parse.h \
    ../libgit2/src/diff_tform.h \
    ../libgit2/src/diff_xdiff.h \
    ../libgit2/src/fetch.h \
    ../libgit2/src/fetchhead.h \
    ../libgit2/src/filebuf.h \
    ../libgit2/src/fileops.h \
    ../libgit2/src/filter.h \
    ../libgit2/src/fnmatch.h \
    ../libgit2/src/global.h \
    ../libgit2/src/hash.h \
    ../libgit2/src/idxmap.h \
    ../libgit2/src/ignore.h \
    ../libgit2/src/index.h \
    ../libgit2/src/integer.h \
    ../libgit2/src/iterator.h \
    ../libgit2/src/khash.h \
    ../libgit2/src/map.h \
    ../libgit2/src/merge.h \
    ../libgit2/src/merge_driver.h \
    ../libgit2/src/message.h \
    ../libgit2/src/mwindow.h \
    ../libgit2/src/netops.h \
    ../libgit2/src/notes.h \
    ../libgit2/src/object.h \
    ../libgit2/src/odb.h \
    ../libgit2/src/offmap.h \
    ../libgit2/src/oid.h \
    ../libgit2/src/oidarray.h \
    ../libgit2/src/oidmap.h \
    ../libgit2/src/openssl_stream.h \
    ../libgit2/src/pack.h \
    ../libgit2/src/pack-objects.h \
    ../libgit2/src/patch.h \
    ../libgit2/src/patch_generate.h \
    ../libgit2/src/patch_parse.h \
    ../libgit2/src/path.h \
    ../libgit2/src/pathspec.h \
    ../libgit2/src/pool.h \
    ../libgit2/src/posix.h \
    ../libgit2/src/pqueue.h \
    ../libgit2/src/proxy.h \
    ../libgit2/src/push.h \
    ../libgit2/src/refdb.h \
    ../libgit2/src/refdb_fs.h \
    ../libgit2/src/reflog.h \
    ../libgit2/src/refs.h \
    ../libgit2/src/refspec.h \
    ../libgit2/src/remote.h \
    ../libgit2/src/repo_template.h \
    ../libgit2/src/repository.h \
    ../libgit2/src/revwalk.h \
    ../libgit2/src/sha1_lookup.h \
    ../libgit2/src/signature.h \
    ../libgit2/src/socket_stream.h \
    ../libgit2/src/sortedcache.h \
    ../libgit2/src/status.h \
    ../libgit2/src/stransport_stream.h \
    ../libgit2/src/stream.h \
    ../libgit2/src/strmap.h \
    ../libgit2/src/strnlen.h \
    ../libgit2/src/submodule.h \
    ../libgit2/src/sysdir.h \
    ../libgit2/src/tag.h \
    ../libgit2/src/thread-utils.h \
    ../libgit2/src/tls_stream.h \
    ../libgit2/src/trace.h \
    ../libgit2/src/transaction.h \
    ../libgit2/src/tree.h \
    ../libgit2/src/tree-cache.h \
    ../libgit2/src/userdiff.h \
    ../libgit2/src/util.h \
    ../libgit2/src/varint.h \
    ../libgit2/src/vector.h \
    ../libgit2/src/zstream.h \
    ../libgit2/deps/regex/config.h \
    ../libgit2/deps/regex/regex.h \
    ../libgit2/deps/regex/regex_internal.h \
    ../libgit2/src/hash/hash_common_crypto.h \
    ../libgit2/src/hash/hash_openssl.h \
    ../libgit2/src/hash/hash_generic.h \
    ../libgit2/deps/zlib/crc32.h \
    ../libgit2/deps/zlib/deflate.h \
    ../libgit2/deps/zlib/inffast.h \
    ../libgit2/deps/zlib/inffixed.h \
    ../libgit2/deps/zlib/inflate.h \
    ../libgit2/deps/zlib/inftrees.h \
    ../libgit2/deps/zlib/trees.h \
    ../libgit2/deps/zlib/zconf.h \
    ../libgit2/deps/zlib/zlib.h \
    ../libgit2/deps/zlib/zutil.h \
    ../libgit2/src/transports/auth.h \
    ../libgit2/src/transports/auth_negotiate.h \
    ../libgit2/src/transports/cred.h \
    ../libgit2/src/transports/smart.h \
    ../libgit2/src/transports/ssh.h \
    ../libgit2/deps/http-parser/http_parser.h \
    ../libgit2/src/xdiff/xdiff.h \
    ../libgit2/src/xdiff/xdiffi.h \
    ../libgit2/src/xdiff/xemit.h \
    ../libgit2/src/xdiff/xinclude.h \
    ../libgit2/src/xdiff/xmacros.h \
    ../libgit2/src/xdiff/xprepare.h \
    ../libgit2/src/xdiff/xtypes.h \
	../libgit2/src/xdiff/xutils.h

unix:HEADERS += \
    ../libgit2/src/unix/posix.h \
    ../libgit2/src/unix/pthread.h

win32:HEADERS += \
	../libgit2/src/win32/dir.h \
	../libgit2/src/win32/error.h \
	../libgit2/src/win32/findfile.h \
	../libgit2/src/win32/mingw-compat.h \
	../libgit2/src/win32/msvc-compat.h \
	../libgit2/src/win32/path_w32.h \
	../libgit2/src/win32/posix.h \
	../libgit2/src/win32/precompiled.h \
	../libgit2/src/win32/reparse.h \
	../libgit2/src/win32/thread.h \
	../libgit2/src/win32/utf-conv.h \
	../libgit2/src/win32/version.h \
	../libgit2/src/win32/w32_buffer.h \
	../libgit2/src/win32/w32_crtdbg_stacktrace.h \
	../libgit2/src/win32/w32_stack.h \
	../libgit2/src/win32/w32_util.h \
	../libgit2/src/win32/win32-compat.h


SOURCES += \
    ../libgit2/src/annotated_commit.c \
    ../libgit2/src/apply.c \
    ../libgit2/src/attr.c \
    ../libgit2/src/attr_file.c \
    ../libgit2/src/attrcache.c \
    ../libgit2/src/blame.c \
    ../libgit2/src/blame_git.c \
    ../libgit2/src/blob.c \
    ../libgit2/src/branch.c \
    ../libgit2/src/buf_text.c \
    ../libgit2/src/buffer.c \
    ../libgit2/src/cache.c \
    ../libgit2/src/checkout.c \
    ../libgit2/src/cherrypick.c \
    ../libgit2/src/clone.c \
    ../libgit2/src/commit.c \
    ../libgit2/src/commit_list.c \
    ../libgit2/src/config.c \
    ../libgit2/src/config_cache.c \
    ../libgit2/src/config_file.c \
    ../libgit2/src/crlf.c \
    ../libgit2/src/curl_stream.c \
    ../libgit2/src/date.c \
    ../libgit2/src/delta.c \
    ../libgit2/src/describe.c \
    ../libgit2/src/diff.c \
    ../libgit2/src/diff_driver.c \
    ../libgit2/src/diff_file.c \
    ../libgit2/src/diff_generate.c \
    ../libgit2/src/diff_parse.c \
    ../libgit2/src/diff_print.c \
    ../libgit2/src/diff_stats.c \
    ../libgit2/src/diff_tform.c \
    ../libgit2/src/diff_xdiff.c \
    ../libgit2/src/errors.c \
    ../libgit2/src/fetch.c \
    ../libgit2/src/fetchhead.c \
    ../libgit2/src/filebuf.c \
    ../libgit2/src/fileops.c \
    ../libgit2/src/filter.c \
    ../libgit2/src/fnmatch.c \
    ../libgit2/src/global.c \
    ../libgit2/src/graph.c \
    ../libgit2/src/hash.c \
    ../libgit2/src/hashsig.c \
    ../libgit2/src/ident.c \
    ../libgit2/src/ignore.c \
    ../libgit2/src/index.c \
    ../libgit2/src/indexer.c \
    ../libgit2/src/iterator.c \
    ../libgit2/src/merge.c \
    ../libgit2/src/merge_driver.c \
    ../libgit2/src/merge_file.c \
    ../libgit2/src/message.c \
    ../libgit2/src/mwindow.c \
    ../libgit2/src/netops.c \
    ../libgit2/src/notes.c \
    ../libgit2/src/object.c \
    ../libgit2/src/object_api.c \
    ../libgit2/src/odb.c \
    ../libgit2/src/odb_loose.c \
    ../libgit2/src/odb_mempack.c \
    ../libgit2/src/odb_pack.c \
    ../libgit2/src/oid.c \
    ../libgit2/src/oidarray.c \
    ../libgit2/src/openssl_stream.c \
    ../libgit2/src/pack.c \
    ../libgit2/src/pack-objects.c \
    ../libgit2/src/patch.c \
    ../libgit2/src/patch_generate.c \
    ../libgit2/src/patch_parse.c \
    ../libgit2/src/path.c \
    ../libgit2/src/pathspec.c \
    ../libgit2/src/pool.c \
    ../libgit2/src/posix.c \
    ../libgit2/src/pqueue.c \
    ../libgit2/src/proxy.c \
    ../libgit2/src/push.c \
    ../libgit2/src/rebase.c \
    ../libgit2/src/refdb.c \
    ../libgit2/src/refdb_fs.c \
    ../libgit2/src/reflog.c \
    ../libgit2/src/refs.c \
    ../libgit2/src/refspec.c \
    ../libgit2/src/remote.c \
    ../libgit2/src/repository.c \
    ../libgit2/src/reset.c \
    ../libgit2/src/revert.c \
    ../libgit2/src/revparse.c \
    ../libgit2/src/revwalk.c \
    ../libgit2/src/settings.c \
    ../libgit2/src/sha1_lookup.c \
    ../libgit2/src/signature.c \
    ../libgit2/src/socket_stream.c \
    ../libgit2/src/sortedcache.c \
    ../libgit2/src/stash.c \
    ../libgit2/src/status.c \
    ../libgit2/src/stransport_stream.c \
    ../libgit2/src/strmap.c \
    ../libgit2/src/submodule.c \
    ../libgit2/src/sysdir.c \
    ../libgit2/src/tag.c \
    ../libgit2/src/thread-utils.c \
    ../libgit2/src/tls_stream.c \
    ../libgit2/src/trace.c \
    ../libgit2/src/transaction.c \
    ../libgit2/src/transport.c \
    ../libgit2/src/tree.c \
    ../libgit2/src/tree-cache.c \
    ../libgit2/src/tsort.c \
    ../libgit2/src/util.c \
    ../libgit2/src/varint.c \
    ../libgit2/src/vector.c \
    ../libgit2/src/zstream.c \
    ../libgit2/deps/regex/regcomp.c \
    ../libgit2/deps/regex/regex.c \
    ../libgit2/deps/regex/regex_internal.c \
    ../libgit2/deps/regex/regexec.c \
    ../libgit2/src/hash/hash_generic.c \
    ../libgit2/deps/zlib/adler32.c \
    ../libgit2/deps/zlib/crc32.c \
    ../libgit2/deps/zlib/deflate.c \
    ../libgit2/deps/zlib/infback.c \
    ../libgit2/deps/zlib/inffast.c \
    ../libgit2/deps/zlib/inflate.c \
    ../libgit2/deps/zlib/inftrees.c \
    ../libgit2/deps/zlib/trees.c \
    ../libgit2/deps/zlib/zutil.c \
    ../libgit2/src/transports/auth.c \
    ../libgit2/src/transports/auth_negotiate.c \
    ../libgit2/src/transports/cred.c \
    ../libgit2/src/transports/cred_helpers.c \
    ../libgit2/src/transports/git.c \
    ../libgit2/src/transports/http.c \
    ../libgit2/src/transports/local.c \
    ../libgit2/src/transports/smart.c \
    ../libgit2/src/transports/smart_pkt.c \
    ../libgit2/src/transports/smart_protocol.c \
    ../libgit2/src/transports/ssh.c \
    ../libgit2/src/transports/winhttp.c \
    ../libgit2/deps/http-parser/http_parser.c \
    ../libgit2/src/xdiff/xdiffi.c \
    ../libgit2/src/xdiff/xemit.c \
    ../libgit2/src/xdiff/xhistogram.c \
    ../libgit2/src/xdiff/xmerge.c \
    ../libgit2/src/xdiff/xpatience.c \
    ../libgit2/src/xdiff/xprepare.c \
	../libgit2/src/xdiff/xutils.c

unix:SOURCES += \
    ../libgit2/src/unix/map.c \
    ../libgit2/src/unix/realpath.c

win32:SOURCES += \
	../libgit2/src/win32/dir.c \
	../libgit2/src/win32/error.c \
	../libgit2/src/win32/findfile.c \
	../libgit2/src/win32/map.c \
	../libgit2/src/win32/path_w32.c \
	../libgit2/src/win32/posix_w32.c \
	../libgit2/src/win32/precompiled.c \
	../libgit2/src/win32/thread.c \
	../libgit2/src/win32/utf-conv.c \
	../libgit2/src/win32/w32_buffer.c \
	../libgit2/src/win32/w32_crtdbg_stacktrace.c \
	../libgit2/src/win32/w32_stack.c \
	../libgit2/src/win32/w32_util.c


