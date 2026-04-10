#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use Sexp;
use Data::Dumper;

my %arg;
use Getopt::Std;
getopts("e",\%arg);
my $expand = defined $arg{e} ? 1 : 0;

sub main () {
  my %dic;
  for my $file (@ARGV) {
    print STDERR "$file ...\n";
    my $file2 = $file;
    $file2 =~ s/.dic$/.csv/;
    my @list = Sexp::parse(file => $file);
    my $c = &read_cforms();
    open (S, "> $file2") || die "$! $file2\n";
    for (my $i = 0; $i <= $#list; $i += 2) {
      for (&parse_morph($list[$i], $list[$i+1])) {
        my $str = &convert($_, $c);
        for (split /\n/, $str) {
          print S "$_\n" if (! defined $dic{$_});
          $dic{$_} = 1;
        }
      }
    }
    close(S);
  }
}

sub getreadings {
  my $s = shift @_;
  my @result = ();
  if (defined $s && $s =~ /^{([^}]+)}([^\}]*)$/) {
  my $stem = $1;
  my $suf = $2;
  for (split /\//, $stem) {
    push @result, $_ . $suf;
  }
  return @result;
} else {
  push @result, $s;
  return @result;
}
}


sub read_cforms {
  my @list = Sexp::parse(file => "cforms.cha");
  my %e = ();
  for my $m (@list) {
    my @l2; 
    for (@{$m->[1]}) {
      push @l2, [ ($_->[0], $_->[1], 
                   $_->[2] ? $_->[2] : $_->[1],
                   $_->[3] ? $_->[3] : ($_->[2] ? $_->[2] : $_->[1])) ];
    }
    $e{$m->[0]} = \@l2;
  }
  return \%e;
}

sub append {
  my ($a, $b) = @_;
  return $b if ($a eq ""); 
  return $a if ($b eq "*" || $b eq "");
  return $a . $b;
}

sub convert {
  my $e       = shift @_;
  my $ctype_  = shift @_;
  my $surface = $e->{surface};
  my $pos1    = $e->{pos1};
  my $pos2    = $e->{pos2};
  my $pos3    = $e->{pos3};
  my $pos4    = $e->{pos4};
  my $ctype   = $e->{ctype};
  my $read    = $e->{read};
  my $pron    = $e->{pron};
  my $cost    = $e->{cost};

  
  if ($ctype eq "*") {
    return sprintf ("%s,0,0,%d,%s,%s,%s,%s,*,*,%s,%s,%s\n",
                    $surface,
                    $cost,
                    $pos1, $pos2, $pos3, $pos4,
                    $surface, $read, $pron);
  }

  my $result = "";
  my $lex   = $surface;
  my $lexs  = $lex;
  my $reads = $read;
  my $prons = $pron;
  
  my @ctype__ = @{$ctype_->{$ctype}};
  for (@ctype__) {
    if ($_->[0] eq "基本形" && $_->[1] ne "*") {
      $lexs  = substr($lex,  0, length($lex)  - length($_->[1]));
      $reads = substr($read, 0, length($read) - length($_->[2]));
      $prons = substr($pron, 0, length($pron) - length($_->[3]));
      last;
    }
  }
  
  my @list = ();
  my $base = "";
  for (@ctype__) {
    my $cform   = $_->[0];
    my $newlex  = &append($lexs,  $_->[1]);
    my $newread = &append($reads, $_->[2]);
    my $newpron = &append($prons, $_->[3]);
    $newread =~ s/\s+//g;
    $newpron =~ s/\s+//g;
    $newlex  =~ s/\s+//g;
    $base = $newlex if ($cform eq "基本形");
    next if (length ($newlex) <= 1);
    push @list, 
    [($newlex,0,0,$cost,$pos1,$pos2,$pos3,$pos4,$ctype,$cform,$lexs,$newread,$newpron)];
  }
  
  for (@list) {
    $_->[10] = $base;
    $result .= ((join ",", @{$_}) . "\n");
  }

  return $result;
}

sub parse_morph() {
  my $m1 = shift @_;
  my $m2 = shift @_;

  my %e = ();    
  $e{surface} = "";
  $e{cost} = 0;
  $e{pos1} = $e{pos2} = $e{pos3} = $e{pos4} =
    $e{ctype} = $e{pron} = $e{read} = "*";

  my @tmp = @{$m1->[1]};
  $e{pos1} = shift @tmp if (@tmp);
  $e{pos2} = shift @tmp if (@tmp);
  $e{pos3} = shift @tmp if (@tmp);
  $e{pos4} = shift @tmp if (@tmp);

  for my $k (@{$m2}) {
    if ($k->[0] eq "見出し語") {
      $e{surface} = $k->[1]->[0];
      $e{cost} = $k->[1]->[1];
    } elsif ($k->[0] eq "読み") {
      $e{read} = $k->[1];
    } elsif ($k->[0] eq "発音") {
      $e{pron} = $k->[1];
    } elsif ($k->[0] eq "活用型") {
      $e{ctype} = $k->[1];
    }
  }

  my @reads = &getreadings($e{read});
  my @prons = &getreadings($e{pron});

  if (!$expand || @reads != @prons) {
    @reads = ( $reads[0] );
    @prons = ( $prons[0] );
  }

  my @result = ();
  for (my $i = 0; $i <= $#reads; ++$i) {
    my %e2 = %e;
    $e2{read} = $reads[$i];
    $e2{pron} = $prons[$i];
    push @result, \%e2;
  }

  return @result;
}

main();
