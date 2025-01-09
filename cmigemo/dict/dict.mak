# vim:set ts=8 sts=8 sw=8 tw=0:
#
# 辞書ファイルのメンテナンス
# 
# Written By:  MURAOKA Taro <koron.kaoriya@gmail.com>

DICT 		= migemo-dict
DICT_BASE	= base-dict
SKKDIC_BASEURL 	= https://skk-dev.github.io/dict
SKKDIC_FILE	= SKK-JISYO.L
EUCJP_DIR	= euc-jp.d
UTF8_DIR	= utf-8.d

##############################################################################
# Dictionary
#
$(DICT): $(DICT_BASE)
	$(FILTER_CP932) < $(DICT_BASE) > $@
$(DICT_BASE): $(SKKDIC_FILE) ../tools/skk2migemo.pl ../tools/optimize-dict.pl
	$(PERL) ../tools/skk2migemo.pl < $(SKKDIC_FILE) > dict.tmp
	$(PERL) ../tools/optimize-dict.pl < dict.tmp > $@
	-$(RM) dict.tmp
$(SKKDIC_FILE):
	$(HTTP) $(SKKDIC_BASEURL)/$@.gz
	$(GUNZIP) $@.gz

##############################################################################
# Dictionary in cp932
#
cp932:		$(DICT)

##############################################################################
# Dictionary in euc-jp
#
euc-jp: 	cp932 euc-jp-files
euc-jp-files: $(EUCJP_DIR)/migemo-dict \
	$(EUCJP_DIR)/zen2han.dat $(EUCJP_DIR)/han2zen.dat \
	$(EUCJP_DIR)/hira2kata.dat $(EUCJP_DIR)/roma2hira.dat
$(EUCJP_DIR):
	$(MKDIR) $(EUCJP_DIR)
$(EUCJP_DIR)/migemo-dict: $(EUCJP_DIR) migemo-dict
	$(FILTER_EUCJP) < migemo-dict > $@
$(EUCJP_DIR)/zen2han.dat: $(EUCJP_DIR) zen2han.dat
	$(FILTER_EUCJP) < zen2han.dat > $@
$(EUCJP_DIR)/han2zen.dat: $(EUCJP_DIR) han2zen.dat
	$(FILTER_EUCJP) < han2zen.dat > $@
$(EUCJP_DIR)/hira2kata.dat: $(EUCJP_DIR) hira2kata.dat
	$(FILTER_EUCJP) < hira2kata.dat > $@
$(EUCJP_DIR)/roma2hira.dat: $(EUCJP_DIR) roma2hira.dat
	$(FILTER_EUCJP) < roma2hira.dat > $@

##############################################################################
# Dictionary in utf-8
#
utf-8: 	cp932 utf-8-files
utf-8-files: $(UTF8_DIR)/migemo-dict \
	$(UTF8_DIR)/zen2han.dat $(UTF8_DIR)/han2zen.dat \
	$(UTF8_DIR)/hira2kata.dat $(UTF8_DIR)/roma2hira.dat
$(UTF8_DIR):
	$(MKDIR) $(UTF8_DIR)
$(UTF8_DIR)/migemo-dict: $(UTF8_DIR) migemo-dict
	$(FILTER_UTF8) < migemo-dict > $@
$(UTF8_DIR)/zen2han.dat: $(UTF8_DIR) zen2han.dat
	$(FILTER_UTF8) < zen2han.dat > $@
$(UTF8_DIR)/han2zen.dat: $(UTF8_DIR) han2zen.dat
	$(FILTER_UTF8) < han2zen.dat > $@
$(UTF8_DIR)/hira2kata.dat: $(UTF8_DIR) hira2kata.dat
	$(FILTER_UTF8) < hira2kata.dat > $@
$(UTF8_DIR)/roma2hira.dat: $(UTF8_DIR) roma2hira.dat
	$(FILTER_UTF8) < roma2hira.dat > $@

##############################################################################
# for Microsoft Visual C
#
msvc:		cp932 utf-8

##############################################################################
# for Borland C 5
#
bc5:		cp932 utf-8

##############################################################################
# for Cygwin
#
cyg:		euc-jp utf-8

##############################################################################
# for MinGW
#
mingw:		cp932 utf-8

##############################################################################
# for GNU/gcc(Linux他)
#
gcc:		euc-jp utf-8

##############################################################################
# for MacOS X
#
osx:		euc-jp utf-8

##############################################################################
# Cleaning
#
dict-clean:
	-$(RM) $(DICT)
	-$(RMDIR) $(EUCJP_DIR)
	-$(RMDIR) $(UTF8_DIR)
dict-distclean: dict-clean
	-$(RM) $(DICT_BASE)
	-$(RM) SKK-JISYO*
