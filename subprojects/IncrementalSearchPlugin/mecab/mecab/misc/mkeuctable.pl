#!/usr/bin/perl

# open (F1, "JIS0201.TXT") || die;
open (F2, "JIS0208.TXT") || die;

while (<F2>) {
    next if /^\#/;
    my ($sjis, $jis, $ucs) = split /\s+/, $_;
    $jis  = hex ($jis);
    $jis += hex (0x8080); # to EUC

    printf "0x%X = %s\n", $jis, $ucs
}

# open (F3, "JIS0212.TXT") || die;

