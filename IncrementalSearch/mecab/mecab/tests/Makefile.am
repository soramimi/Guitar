# Generated automatically from Makefile.in by configure.x
TESTS = run-dics.sh run-eval.sh run-cost-train.sh
EXTRA_DIR = eval autolink dic eval katakana latin shiin t9 chartype cost-train ngram
EXTRA_DIST = $(TESTS)

dist-hook:
	for subdir in $(EXTRA_DIR); do \
	  cp -rp $$subdir $(distdir); \
	  rm -f $(distdir)/$$subdir/*~; \
	  rm -f $(distdir)/$$subdir/*.{bak,orig}; \
	  rm -rf $(distdir)/$$subdir/CVS; \
	  rm -rf $(distdir)/$$subdir/*/CVS; \
	  rm -rf $(distdir)/$$subdir/.svn; \
	  rm -rf $(distdir)/$$subdir/*/.svn; \
	  rm -rf $(distdir)/$$subdir/*/*/.svn; \
	done

clean:
	rm -f tmp.*
