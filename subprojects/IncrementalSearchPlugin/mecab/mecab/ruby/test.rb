#!/usr/bin/ruby
# -*- coding: utf-8 -*-

require 'MeCab'
sentence = "太郎はこの本を二郎を見た女性に渡した。"

begin
  
  print MeCab::VERSION, "\n"	
  model = MeCab::Model.new(ARGV.join(" "))
  tagger = model.createTagger()

  puts tagger.parse(sentence)

  n = tagger.parseToNode(sentence)

  while n do
    print n.surface,  "\t", n.feature, "\t", n.cost, "\n"
    n = n.next
  end
  print "EOS\n";

  lattice = MeCab::Lattice.new()
  lattice.set_sentence(sentence)
  tagger.parse(lattice)
  len = lattice.size()
  for i in 0..len
    b = lattice.begin_nodes(i)
    while b do
      printf "B[%d] %s\t%s\n", i, b.surface, b.feature;
      b = b.bnext 
    end
    e = lattice.end_nodes(i)
    while e do
      printf "E[%d] %s\t%s\n", i, e.surface, e.feature;
      e = e.bnext 
    end  
  end
  print "EOS\n";

  lattice.set_sentence(sentence)
  lattice.set_request_type(MeCab::MECAB_NBEST)
  tagger.parse(lattice)
  for i in 0..10
    lattice.next()
    print lattice.toString()
  end
  
  d = model.dictionary_info()
  while d do
    printf "filename: %s\n", d.filename
    printf "charset: %s\n", d.charset
    printf "size: %d\n", d.size
    printf "type: %d\n", d.type
    printf "lsize: %d\n", d.lsize
    printf "rsize: %d\n", d.rsize
    printf "version: %d\n", d.version
    d = d.next
  end
  
rescue
  print "RuntimeError: ", $!, "\n";
end
