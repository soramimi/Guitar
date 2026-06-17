#!/usr/bin/perl

use lib "../src/.libs";
use lib $ENV{PWD} . "/blib/lib";
use lib $ENV{PWD} . "/blib/arch";
use MeCab;

print $MeCab::VERSION, "\n";

my $sentence = "太郎はこの本を二郎を見た女性に渡した。";

my $model = new MeCab::Model(join " ", @ARGV);
my $c = $model->createTagger();

print $c->parse($sentence);

for (my $m = $c->parseToNode($sentence); $m; $m = $m->{next}) {
    printf("%s\t%s\n", $m->{surface}, $m->{feature});
}

my $lattice = new MeCab::Lattice();
$lattice->set_sentence($sentence);

if ($c->parse($lattice)) {
    for (my $i = 0; $i < $lattice->size(); ++$i) {
	for (my $b = $lattice->begin_nodes($i); $b; $b = $b->{bnext}) {
	    printf("B[%d] %s\t%s\n", $i, $b->{surface}, $b->{feature});
	}
	for (my $e = $lattice->end_nodes($i); $e; $e = $e->{enext}) {
	    printf("E[%d] %s\t%s\n", $i, $e->{surface}, $e->{feature});
	}
    }
}

$lattice->set_sentence($sentence);
$lattice->set_request_type($MeCab::MECAB_NBEST);
if ($c->parse($lattice)) {
    for (my $i = 0; $i < 10; ++$i) {
	$lattice->next();
	print $lattice->toString();
    }
}

for (my $d = $c->dictionary_info(); $d; $d = $d->{next}) {
    printf("filename: %s\n", $d->{filename});
    printf("charset: %s\n", $d->{charset});
    printf("size: %d\n", $d->{size});
    printf("type: %d\n", $d->{type});
    printf("lsize: %d\n", $d->{lsize});
    printf("rsize: %d\n", $d->{rsize});
    printf("version: %d\n", $d->{version});
}
