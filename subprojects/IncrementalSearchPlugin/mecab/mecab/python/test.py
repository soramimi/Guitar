#!/usr/bin/python
# -*- coding: utf-8 -*-

import MeCab
import sys
import string

sentence = "太郎はこの本を二郎を見た女性に渡した。"

try:

    print MeCab.VERSION

    t = MeCab.Tagger (" ".join(sys.argv))

    print t.parse(sentence)

    m = t.parseToNode(sentence)
    while m:
	print m.surface, "\t", m.feature
	m = m.next
    print "EOS"
    
    lattice = MeCab.Lattice()
    t.parse(lattice)
    lattice.set_sentence(sentence)
    len = lattice.size()
    for i in range(len + 1):
        b = lattice.begin_nodes(i)
        e = lattice.end_nodes(i)
        while b:
            print "B[%d] %s\t%s" % (i, b.surface, b.feature)
            b = b.bnext 
        while e:
            print "E[%d] %s\t%s" % (i, e.surface, e.feature)
            e = e.bnext 
    print "EOS";

    d = t.dictionary_info()
    while d:
        print "filename: %s" % d.filename
        print "charset: %s" %  d.charset
        print "size: %d" %  d.size
        print "type: %d" %  d.type
        print "lsize: %d" %  d.lsize
        print "rsize: %d" %  d.rsize
        print "version: %d" %  d.version
        d = d.next

except RuntimeError, e:
    print "RuntimeError:", e;

