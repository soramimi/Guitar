# vim:set ts=8 sts=8 sw=8 tw=0:
#
# Clean up アーキテクチャ非依存
#
# Last Change:	29-Nov-2003.
# Written By:	MURAOKA Taro <koron@tka.att.ne.jp>

clean-migemo:
	-$(RM) $(DICT_DIR)migemo-dict

distclean-migemo:
	-$(RM) $(outdir)cmigemo$(EXEEXT)
	-$(RM) libmigemo.*.dylib
	-$(RM) libmigemo.so*
	-$(RM) migemo.opt
	-$(RM) migemo.ncb
	-$(RMDIR) $(objdir)
	-$(RM) $(DICT_DIR)SKK-JISYO*
	-$(RM) $(DICT_DIR)base-dict
	-$(RMDIR) $(DICT_DIR)euc-jp.d
	-$(RMDIR) $(DICT_DIR)utf-8.d

clean: clean-arch clean-migemo
	-$(RM) *.a
	-$(RM) $(OBJ)
	-$(RM) *.lib
	-$(RM) *.tds
	-$(RMDIR) $(objdir)
	-$(RMDIR) Release
	-$(RMDIR) Debug

distclean: clean distclean-arch distclean-migemo
	-$(RM) *.dll
	-$(RM) *.dylib
	-$(RM) tags
	-$(CP) $(CONFIG_DEFAULT) config.mk
