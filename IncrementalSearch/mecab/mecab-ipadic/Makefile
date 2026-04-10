# Makefile.in generated automatically by automake 1.4-p6 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999, 2001 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/bash

srcdir = .
top_srcdir = .

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${datarootdir}
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${datarootdir}/info
mandir = ${datarootdir}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/mecab-ipadic
pkglibdir = $(libdir)/mecab-ipadic
pkgincludedir = $(includedir)/mecab-ipadic

top_builddir = .

ACLOCAL = /Guitar/IncrementalSearch/mecab/mecab-ipadic/missing aclocal-1.4
AUTOCONF = autoconf
AUTOMAKE = /Guitar/IncrementalSearch/mecab/mecab-ipadic/missing automake-1.4
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = 
host_triplet = @host@
CC = @CC@
CHARSET = utf8
HAVE_LIB = @HAVE_LIB@
LIB = @LIB@
LTLIB = @LTLIB@
MAKEINFO = /Guitar/IncrementalSearch/mecab/mecab-ipadic/missing makeinfo
MECAB_CONFIG = /usr/bin/mecab-config
MECAB_DICDIR = /usr/lib/x86_64-linux-gnu/mecab/dic/ipadic
MECAB_DICT_INDEX = /usr/lib/mecab/mecab-dict-index
MECAB_GENDATA = matrix.bin char.bin sys.dic unk.dic
MECAB_LEXICAL_DIC = Adj.csv Adnominal.csv Adverb.csv Auxil.csv Conjunction.csv Filler.csv Interjection.csv Noun.adjv.csv Noun.adverbal.csv Noun.csv Noun.demonst.csv Noun.nai.csv Noun.name.csv Noun.number.csv Noun.org.csv Noun.others.csv Noun.place.csv Noun.proper.csv Noun.verbal.csv Others.csv Postp-col.csv Postp.csv Prefix.csv Suffix.csv Symbol.csv Verb.csv
MECAB_MECABRC = /etc/mecabrc
MECAB_PREDATA = char.def feature.def left-id.def matrix.def pos-id.def rewrite.def right-id.def unk.def dicrc
PACKAGE = mecab-ipadic
VERSION = 2.7.0-20070801
datarootdir = ${prefix}/share

mecab_dict_index = /usr/lib/mecab/mecab-dict-index
dicdir = /usr/lib/x86_64-linux-gnu/mecab/dic/ipadic

# SUBDIRS    = doc script
dic_DATA = matrix.bin char.bin sys.dic unk.dic left-id.def right-id.def rewrite.def pos-id.def dicrc
EXTRA_DIST = Adj.csv Adnominal.csv Adverb.csv Auxil.csv Conjunction.csv Filler.csv Interjection.csv Noun.adjv.csv Noun.adverbal.csv Noun.csv Noun.demonst.csv Noun.nai.csv Noun.name.csv Noun.number.csv Noun.org.csv Noun.others.csv Noun.place.csv Noun.proper.csv Noun.verbal.csv Others.csv Postp-col.csv Postp.csv Prefix.csv Suffix.csv Symbol.csv Verb.csv char.def feature.def left-id.def matrix.def pos-id.def rewrite.def right-id.def unk.def dicrc RESULT
CLEANFILES = matrix.bin char.bin sys.dic unk.dic
ACLOCAL_M4 = $(top_srcdir)/aclocal.m4
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_CLEAN_FILES = 
DATA =  $(dic_DATA)

DIST_COMMON =  README AUTHORS COPYING ChangeLog INSTALL Makefile.am \
Makefile.in NEWS aclocal.m4 config.guess config.sub configure \
configure.in install-sh missing mkinstalldirs


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = tar
GZIP_ENV = --best
all: all-redirect
.SUFFIXES:
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu --include-deps Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$@ CONFIG_HEADERS= $(SHELL) ./config.status

$(ACLOCAL_M4):  configure.in 
	cd $(srcdir) && $(ACLOCAL)

config.status: $(srcdir)/configure $(CONFIG_STATUS_DEPENDENCIES)
	$(SHELL) ./config.status --recheck
$(srcdir)/configure: $(srcdir)/configure.in $(ACLOCAL_M4) $(CONFIGURE_DEPENDENCIES)
	cd $(srcdir) && $(AUTOCONF)

install-dicDATA: $(dic_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(dicdir)
	@list='$(dic_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(dicdir)/$$p"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(dicdir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(dicdir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(dicdir)/$$p; \
	  fi; fi; \
	done

uninstall-dicDATA:
	@$(NORMAL_UNINSTALL)
	list='$(dic_DATA)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(dicdir)/$$p; \
	done
tags: TAGS
TAGS:


distdir = $(PACKAGE)-$(VERSION)
top_distdir = $(distdir)

# This target untars the dist file and tries a VPATH configuration.  Then
# it guarantees that the distribution is self-contained by making another
# tarfile.
distcheck: dist
	-rm -rf $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) zxf $(distdir).tar.gz
	mkdir $(distdir)/=build
	mkdir $(distdir)/=inst
	dc_install_base=`cd $(distdir)/=inst && pwd`; \
	cd $(distdir)/=build \
	  && ../configure --srcdir=.. --prefix=$$dc_install_base \
	  && $(MAKE) $(AM_MAKEFLAGS) \
	  && $(MAKE) $(AM_MAKEFLAGS) dvi \
	  && $(MAKE) $(AM_MAKEFLAGS) check \
	  && $(MAKE) $(AM_MAKEFLAGS) install \
	  && $(MAKE) $(AM_MAKEFLAGS) installcheck \
	  && $(MAKE) $(AM_MAKEFLAGS) dist
	-rm -rf $(distdir)
	@banner="$(distdir).tar.gz is ready for distribution"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"
dist: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	-rm -rf $(distdir)
dist-all: distdir
	-chmod -R a+r $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) chozf $(distdir).tar.gz $(distdir)
	-rm -rf $(distdir)
distdir: $(DISTFILES)
	-rm -rf $(distdir)
	mkdir $(distdir)
	-chmod 777 $(distdir)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am:
install-exec: install-exec-am

install-data-am: install-dicDATA
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-dicDATA
uninstall: uninstall-am
all-am: Makefile $(DATA)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(dicdir)


mostlyclean-generic:

clean-generic:
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-generic clean-am

distclean: distclean-am
	-rm -f config.status

maintainer-clean-am:  maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am
	-rm -f config.status

.PHONY: uninstall-dicDATA install-dicDATA tags distdir info-am info \
dvi-am dvi check check-am installcheck-am installcheck install-exec-am \
install-exec install-data-am install-data install-am install \
uninstall-am uninstall all-redirect all-am all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


matrix.bin char.bin sys.dic unk.dic:
	$(mecab_dict_index) -d . -o . -f EUC-JP -t utf8
	echo To enable dictionary, rewrite /etc/mecabrc as \"dicrc = /usr/lib/x86_64-linux-gnu/mecab/dic/ipadic\"

rpm: dist
	rpm -ta mecab-ipadic-2.7.0-20070801.tar.gz

export-package:
	./upload.pl -p mecab -n mecab-ipadic -r 2.7.0-20070801 -f mecab-ipadic-2.7.0-20070801.tar.gz

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
