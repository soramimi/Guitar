#!/usr/bin/env perl

use FindBin qw($Bin);
use lib "$Bin";
use Sexp;
use Data::Dumper;

sub main () {
    for my $file (@ARGV) {
	print STDERR "$file ...\n";
	my $file2 = $file;
	$file2 =~ s/.dic$/.csv/;
	my @list = Sexp::parse(file => $file);

	my $c = &read_ctype();
	open (S, "> $file2") || die "$! $file2\n";
	for my $m (@list) {
	    for (&parse_morph($m)) {
		my $str = &convert($_, $c);
		for (split /\n/, $str) {
		    if (! defined $dic{$_}) {
			print S "$_\n";
		    }
		    $dic{$_} = 1;
		}
	    }
	}
	close(S);
    }
}

sub read_ctype {
    my @list = Sexp::parse(file => "JUMAN.katuyou");
    my %e = ();
    for my $m (@list) {
	my @l = @{$m->[1]};
	my @l2; 
	for (@l) {
	    push @l2, [ ($_->[0], $_->[1], $_->[2] ? $_->[2] : $_->[1]) ];
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
    my $ctype   = $e->{ctype};
    my $reading = $e->{reading};
    my $meaning = $e->{meaning};

    if ($ctype eq "*") {
	my $result = "";
	for (@$surface) {
	    $result .= sprintf ("%s,0,0,0,%s,%s,*,*,%s,%s,%s\n",
		$_, $pos1, $pos2, $_, $reading, $meaning);
	}
	return $result;
    }

    my $result = "";
    for my $s (@$surface) {
	my $lex   = $s;
	my $lexs  = $lex;
	my $sem   = $meaning;
	my $read  = $reading;	
	my $reads = $read;
    
	my @ctype__ = @{$ctype_->{$ctype}};
	for (@ctype__) {
	    if ($_->[0] eq "基本形") {
		$lexs  = substr ($lex,  0, length ($lex)  - length($_->[1]));
		$reads = substr ($read, 0, length ($read) - length($_->[2]));
		last;
	    }
	}
	
	my @list = ();
	my $base = "";
	for (@ctype__) {
	    my $cform   = $_->[0];
	    my $newlex  = &append ($lexs,  $_->[1]);
	    my $newread = &append ($reads, $_->[2]);
	    $newread =~ s/\s+//g;
	    $newlex  =~ s/\s+//g;
	    $base = $newlex if ($cform eq "基本形");
	    next if (length ($newlex) <= 1);
	    push @list, 
	      [($newlex,0,0,0,$pos1,$pos2,$ctype,$cform,$lexs,$newread,$sem)];
	}
	
	for (@list) {
	    $_->[8] = $base;
	    $result .= ((join ",", @{$_}) . "\n");
	}
    }
    
    return $result;
}

sub parse_morph() {
    my $m = shift @_;

    my @list = ();
    if ($m->[0] eq "連語") {
	@list = @{$m->[1]};
    } else {
	push @list, $m;
    }
    
    my @result;

    for my $m (@list) {
	my %e = ();
	
	my $pos1 = $m->[0];
	my $pos2 = "*";
	my $n = 0;
	if (ref($m->[1]->[0]) eq 'ARRAY') {
	    $n = $m;
	} else {
	    $pos2 = $m->[1]->[0];
	    $n = $m->[1];
	}
	
	shift(@{$n});
	for my $n2 (@{$n}) {
	    my %e;
	    $e{pos1} = $pos1;
	    $e{pos2} = $pos2;
	    $e{ctype} = $e{meaning} = $e{reading} = "*";
	    for my $k (@{$n2}) {
		if ($k->[0] eq "見出し語") {
		    for (my $i = 1; $i <= $#$k; ++$i) {
			if (ref ($k->[$i]) eq 'ARRAY') {
			    push @{$e{surface}}, $k->[$i]->[0];
			} else {
			    push @{$e{surface}}, $k->[$i];
			}
		    }
		} elsif ($k->[0] eq "読み") {
		    $e{reading} = $k->[1];
		} elsif ($k->[0] eq "活用型") {
		    $e{ctype} = $k->[1];
		} elsif ($k->[0] eq "意味情報") {
		    $k->[1] =~ s/\"//g; 
		    $e{meaning} = $k->[1];
		}
	    }
	    
	    push @result, \%e;
	}
    }
	
    return @result;
}

main();
