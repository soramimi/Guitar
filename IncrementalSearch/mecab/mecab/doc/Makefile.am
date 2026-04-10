# Process this file with automake to produce Makefile.in

html_HTMLS = mecab.html
EXTRA_DIST = $(html_HTMLS) bindings.html dic.html format.html \
             learn.html mecab.html unk.html partial.html \
	     dic-detail.html  feature.html index.html \
	     libmecab.html  soft.html posid.html mecab.css \
	     feature.png  flow.png  result.png en doxygen

update-html:
	@for html in $(html_HTMLS); do \
		cmd=`basename $$html .html`.1 && \
		man2html ../man/$$cmd > $$html; \
	done
