#!/usr/bin/perl
use strict;
use warnings;

sub main {
    if (@ARGV != 2) {
        print STDERR "argument count mismatch\n";
        print STDERR "usage: $0 <file> <symbol>\n";
        exit 1;
    }

    my ($path, $symbol) = @ARGV;

    # ファイルを開く
    open my $fh, '<', $path or do {
        print STDERR "open: $!\n";
        exit 1;
    };

    # バイナリモードに設定
    binmode $fh;

    # ファイル全体を読み込む
    my $buffer;
    {
        local $/;  # スラープモード
        $buffer = <$fh>;
    }
    close $fh;

    # ファイルサイズを取得
    my $size = length $buffer;

    # C配列として出力
    print "char const ${symbol}_data[] = {\n";
    
    for my $i (0 .. $size - 1) {
        if ($i % 16 == 0 && $i != 0) {
            print "\n";
        }
        printf "0x%02x, ", ord(substr($buffer, $i, 1));
    }
    
    print "\n};\n";
    print "int ${symbol}_size = $size;\n";

    return 0;
}

exit main();
