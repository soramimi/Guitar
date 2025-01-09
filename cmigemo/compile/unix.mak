# vim:set ts=8 sts=8 sw=8 tw=0:
#
# UNIXŒn‹¤’ÊMakefile
#
# Last Change:	08-Dec-2004.
# Base Idea:	AIDA Shinra
# Maintainer:	MURAOKA Taro <koron@tka.att.ne.jp>

libmigemo_SRC = $(SRC)
libmigemo_OBJ = $(OBJ)

DEFINES	=
CFLAGS	= -O2 -Wall $(DEFINES) $(CFLAGS_MIGEMO)
LDFLAGS = $(LDFLAGS_MIGEMO)
LIBS	= 

default: $(outdir)cmigemo$(EXEEXT)

dirs:
	@for i in $(objdir) $(outdir); do \
		if test ! -d $$i; then \
			$(MKDIR) $$i; \
		fi \
	done

$(outdir)cmigemo$(EXEEXT): $(objdir)main.$(O) $(libmigemo_LIB)
	$(CC) -o $@ $(objdir)main.$(O) -L. -L$(outdir) -lmigemo $(LDFLAGS)

$(objdir)main.o: $(srcdir)main.c dirs
	$(CC) $(CFLAGS) -o $@ -c $<

$(objdir)%.o: $(srcdir)%.c dirs
	$(CC) $(CFLAGS) -o $@ -c $<

##############################################################################
# Install
#
install-mkdir:
	$(MKDIR) $(bindir)
	$(MKDIR) $(libdir)
	$(MKDIR) $(incdir)
	$(MKDIR) $(docdir)
	$(MKDIR) $(dictdir)
	$(MKDIR) $(dictdir)/cp932
	$(MKDIR) $(dictdir)/euc-jp
	$(MKDIR) $(dictdir)/utf-8

install-dict: install-mkdir
	$(INSTALL_DATA) dict/migemo-dict $(dictdir)/cp932
	$(INSTALL_DATA) dict/han2zen.dat $(dictdir)/cp932
	$(INSTALL_DATA) dict/hira2kata.dat $(dictdir)/cp932
	$(INSTALL_DATA) dict/roma2hira.dat $(dictdir)/cp932
	$(INSTALL_DATA) dict/zen2han.dat $(dictdir)/cp932
	if [ -d dict/euc-jp.d ]; then \
	  $(INSTALL_DATA) dict/euc-jp.d/migemo-dict $(dictdir)/euc-jp; \
	  $(INSTALL_DATA) dict/euc-jp.d/han2zen.dat $(dictdir)/euc-jp; \
	  $(INSTALL_DATA) dict/euc-jp.d/hira2kata.dat $(dictdir)/euc-jp; \
	  $(INSTALL_DATA) dict/euc-jp.d/roma2hira.dat $(dictdir)/euc-jp; \
	  $(INSTALL_DATA) dict/euc-jp.d/zen2han.dat $(dictdir)/euc-jp; \
	fi
	if [ -d dict/utf-8.d ]; then \
	  $(INSTALL_DATA) dict/utf-8.d/migemo-dict $(dictdir)/utf-8; \
	  $(INSTALL_DATA) dict/utf-8.d/han2zen.dat $(dictdir)/utf-8; \
	  $(INSTALL_DATA) dict/utf-8.d/hira2kata.dat $(dictdir)/utf-8; \
	  $(INSTALL_DATA) dict/utf-8.d/roma2hira.dat $(dictdir)/utf-8; \
	  $(INSTALL_DATA) dict/utf-8.d/zen2han.dat $(dictdir)/utf-8; \
	fi

# depends on $(libdir) to be already present
install-lib: install-mkdir

install: $(outdir)cmigemo$(EXEEXT) $(libmigemo_DSO) install-mkdir install-dict install-lib
	$(INSTALL_DATA) $(srcdir)migemo.h $(incdir)
	$(INSTALL_DATA) doc/README_j.txt $(docdir)
	$(INSTALL_PROGRAM) $(outdir)cmigemo$(EXEEXT) $(bindir)

##############################################################################
# Uninstall
#
uninstall: uninstall-lib
	$(RM) $(dictdir)/migemo-dict*
	$(RM) $(incdir)/migemo.h
	$(RM) $(docdir)/README_j.txt
	$(RM) $(bindir)/cmigemo$(EXEEXT)
	$(RMDIR) $(dictdir)
	$(RMDIR) $(docdir)
