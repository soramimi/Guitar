#!/usr/bin/perl
use strict;
use warnings;

# Usage: newxxd.pl -n <symbol_name> <input_file>
#   -n <symbol_name>  : C array variable name (required)
#   <input_file>      : input file (required)

my $symbol = undef;
my $infile = undef;

while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg eq '-n') {
        $symbol = shift @ARGV;
    } else {
        $infile = $arg;
    }
}

die "Usage: $0 -n <symbol_name> <input_file>\n"
    unless defined $symbol && defined $infile;

open(my $fh, '<:raw', $infile) or die "Cannot open '$infile': $!\n";
my $data;
{
    local $/;
    $data = <$fh>;
}
close($fh);

my @bytes = unpack('C*', $data);
my $len   = scalar @bytes;

print "unsigned char ${symbol}[] = {\n";

my $i = 0;
while ($i < $len) {
    print "  ";
    my $end = ($i + 12 < $len) ? $i + 12 : $len;
    my @chunk = @bytes[$i .. $end - 1];
    print join(', ', map { sprintf("0x%02x", $_) } @chunk);
    print ',' if $end < $len;
    print "\n";
    $i = $end;
}

print "};\n";
print "unsigned int ${symbol}_len = ${len};\n";
