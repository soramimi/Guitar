#!/usr/bin/perl -w

my $FACTOR = 200;
use MeCab;

sub toCost()
{
    my $prob = shift @_;
    return int(-log($prob) * $FACTOR);
}

my $mecab = new MeCab::Tagger("-d dic");

while (<>) {
    chomp;
    next if /^EOS/;
    s/^\s+//g;
    my ($freq, $s) = split;

    my @list;
    push @list, [ ("_", "_") ];
    for (split /\n/, $mecab->parse($s)) {
	last if /EOS/;
	my ($w, $r) = split;
	goto NEXT if ($r eq "*");
	push @list, [ ($r, $w) ];
    }
    push @list, [ ("_", "_") ];

    for (my $i = 0; $i < $#list; ++$i) {
	my ($n0, $w0) = @{$list[$i]};
	my ($n1, $w1) = @{$list[$i+1]};
	$e1{$w0} += $freq;
	$e2{$w0}{$w1} += $freq;
    }

  NEXT:
}

my %e3;
my $id = 0;
for (sort keys %e1) {
    $e3{$_} = $id;
    ++$id;
}

open (S, "> dic.csv") || die;
for my $w0 (keys %e1) {
    my $tmp =  (split /\n/, $mecab->parse($w0))[0];
    my ($w, $n) = split /\s+/, $tmp;
    next if ($n eq "*");
    printf S "%s,%d,%d,0,%s\n", $n, $e3{$w0}, $e3{$w0}, $w0;
}
close (S);

open(S, "> matrix.def") || die;
my $size = scalar keys %e3;
print S "$size $size\n";
my @l = sort keys %e3;
for my $w0 (@l) {
    my $f1 = $e1{$w0} || 0.0;
    for my $w1 (@l) {
	my $f2 = $e2{$w0}{$w1} || 0.0;;
	my $c = &toCost(1.0 * ($f2 + 0.5) / ($f1 + $size * 0.5));
	printf S "%d %d %d\n", $e3{$w0}, $e3{$w1}, $c;
    }
}
close(S);
