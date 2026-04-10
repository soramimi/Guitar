#!/usr/bin/perl

# 新 しん * 接頭辞 名詞接頭辞
#  あたり あたり あたる 動詞 * 子音動詞ラ行 基本連用形
while (<>) {
    chomp;
    next if (/^#/);
    next if (/^\*/);
    if (/EOS/) {
	print "EOS\n";
    } else {
	my @t = split /\s+/, $_;
	$t[2] = $t[0] if ($t[2] eq "*");
        print "$t[0]\t$t[3],$t[4],$t[5],$t[6],$t[2],$t[1]\n";
    }
}
