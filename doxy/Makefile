all:
	rm -fr /tmp/guitar_src_docs
	doxygen ./Doxy.conf
	tar acvf guitar-doxy-html.tar.zst html

clean:
	rm -fr html latex
	-rm guitar-doxy-html.tar.zst


