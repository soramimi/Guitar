#!/usr/bin/perl -w

my $FACTOR = 200;

sub toCost()
{
    my $prob = shift @_;
    return int(-log($prob) * $FACTOR);
}

sub char2num {
    my $n = ord(shift @_);
    return 1 if ($n >= 0x30A1 && $n <= 0x30AA);
    return 2 if ($n >= 0x30AB && $n <= 0x30B4);
    return 3 if ($n >= 0x30B5 && $n <= 0x30BE);
    return 4 if ($n >= 0x30BF && $n <= 0x30C9);
    return 5 if ($n >= 0x30CA && $n <= 0x30CE);
    return 6 if ($n >= 0x30CF && $n <= 0x30DD);
    return 7 if ($n >= 0x30DF && $n <= 0x30E2);
    return 8 if ($n >= 0x30E3 && $n <= 0x30E8);
    return 9 if ($n >= 0x30E9 && $n <= 0x30ED);
    return 0 if ($n >= 0x30EE && $n <= 0x30F3);
    return 0 if ($n == 0x30FC);
    return -1;
}


while (<>) {
    chomp;
    next if /^EOS/;
    s/^\s+//g;
    my ($freq, $s) = split;
    utf8::decode($s);
    my $length = length($s);
    
    my @list;
    push @list, [ (-1, "_") ];
    for (my $i = 0; $i < $length; ++$i) {
	my $k = substr($s, $i, 1);
	my $n = &char2num($k);
	utf8::encode($k);
	goto NEXT if ($n == -1);
	push @list, [ ($n, $k) ];
    }

    push @list, [ (-1, "_") ];

    for (my $i = 0; $i < $#list; ++$i) {
	my ($n0, $w0) = @{$list[$i]};
	my ($n1, $w1) = @{$list[$i+1]};
	$e1{$w0} += $freq;
	$e2{$w0}{$w1} += $freq;
    }

  NEXT:
}

# mkid
my %e3;
my $id = 0;
for (sort keys %e1) {
    $e3{$_} = $id;
    ++$id;
}

open (S, "> dic.csv") || die;
for $w0 (keys %e1) {
    my $k = $w0;
    utf8::decode($k);
    my $n = &char2num($k);
    next if ($n == -1);
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
