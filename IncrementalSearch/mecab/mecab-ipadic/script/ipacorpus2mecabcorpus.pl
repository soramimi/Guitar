#!/usr/bin/perl

# 行く    イク    行く    動詞-自立       五段・カ行促音便        基本形

# ついて ついて つく 動詞 * 子音動詞カ行 タ系連用テ形

while (<>) {
    chomp;
    next if (/^\*/ || /^#/);
    
    if (/EOS/) {
	print "EOS\n";
    } else {
	my @w = split;
	
	for my $i (0..6) {
	    $w[$i] = "*" if ($w[$i] eq "");
	}
	my @w2 = split /-/, $w[4];
	for my $i (0..3) {
	    $w2[$i] = "*" if (! defined $w2[$i])
	}

	print "$w[0]\t$w2[0],$w2[1],$w2[2],$w2[3],$w[5],$w[6],$w[3],$w[1],$w[2]\n";
    }
}  


