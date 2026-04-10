package Sexp;
use Carp;
use IO::File;
use strict;
use base qw/ Exporter /;
use vars qw/ @EXPORT_OK /;
@EXPORT_OK = qw/ parse /;

=head1 NAME

Juman::Sexp - S式を読み込むモジュール

=head1 SYNOPSIS

 use Data::Dumper;
 use Juman::Sexp qw/ parse /;
 print &Dumper( &parse( file => "Noun.dic" ) );

=head1 DESCRIPTION

C<Juman::Sexp> は，Juman 辞書や設定ファイルに用いられているS式を読み込
むための関数 C<parse> を定義している．

=head1 FUNCTIONS

=over 4

=item parse

指定された対象を，S式として解析する関数．以下のオプションを受け付ける．

=over 4

=item file => FILE

解析するファイルを指定する．

=item string => STRING

解析する文字列を指定する．

=item comment => STRING

コメント開始文字列を指定する．コメントをまったく含まない対象を解析する
場合は，以下のように未定義値を指定する．

  Example:

    &parse( file => "example.dat", comment => undef );

=item debug => BOOLEAN

デバッグ用の情報を出力するように指示する．

=back

=back

例えば，文字列を対象として解析する場合は，以下のように指定する．

  Example:

    &parse( string =>
            "(名詞 (普通名詞 ((読み かめ)(見出し語 亀 かめ カメ))))" );

この場合，次のような解析結果が返される．

    ( [ '名詞',
         [ '普通名詞',
           [ [ '読み', 'かめ' ],
             [ '見出し語', '亀', 'かめ', 'カメ' ]
           ]
         ]
       ] )

=cut
sub parse {
    my %option;
    $option{comment} = ";";
    while( @_ ){
	my $key = shift;
	my $val = shift;
	$key =~ s/\A-+//;
	$option{lc($key)} = $val;
    }
    if( $option{file} ){
	my $file = $option{file};
	if( my $fh = IO::File->new( $file, "r" ) ){
	    my $num = 0;
	    &_parse( sub { if( $fh->eof ){ undef; } else { $num++; $fh->getline; } },
		     sub { "at $file line $num"; },
		     $option{comment},
		     $option{debug} );
	} else {
	    warn "Cannot open $file: $!\n";
	    wantarray ? () : 0;
	}
    } elsif( $option{string} ){
	my $string = $option{string};
	&_parse( sub { my $x = $string; $string = undef; $x; },
		 sub { "in string"; },
		 $option{comment},
		 $option{debug} );
    } else {
	carp "Neither `file' option nor `string' option is specified";
	wantarray ? () : 0;
    }
}

sub _parse {
    my( $getline, $place, $comment, $debug ) = @_;
    my $str = "";
    my @stack;		# shift-reduce 法で構文解析するためのスタック 
    my @offset;		# reduce すべき要素数を記録しておくカウンタ
    while(1){
	$str =~ s/\A\s*//;
	$str =~ s/\A$comment[^\n]*\n\s*// if $comment;
	if( ! $str ){
	    if( $str = &$getline() ){
		print STDERR "PARSE: $str" if $debug;
	    } else {
		if( @offset ){
		    die "Syntax error: end of target during parsing.\n";
		} else {
		    last;
		}
	    }
	}
	# 開括弧を shift する
	elsif( $str =~ s/\A\(// ){
	    $offset[0]-- if @offset;
	    unshift( @offset, 0 );
	}
	# 文字列を shift する
	elsif( $str =~ m/\A"/ ){
	    while(1){
		if( $str =~ s/\A("(?:[^"\\]+|\\.)*")// ){
		    $offset[0]--;
		    push( @stack, $1 );
		    last;
		} elsif( my $next = &$getline() ){
		    $str .= $next;
		} else {
		    die "Syntax error: end of target during string.\n";
		}
	    }
	}
	# シンボルを shift する
	elsif( $str =~ s/\A([^\s"()]+)// ){
	    $offset[0]--;
	    push( @stack, $1 );
	}
	# 閉括弧(= リスト)を reduce する
	elsif( $str =~ s/\A\)// ){
	    unless( @offset ){
		die( "Syntax error: too much close brackets ", &$place(), ".\n" );
	    } else {
		my $offset = shift @offset;
		if( $offset < 0 ){
		    push( @stack, [ splice( @stack, $offset ) ] );
		} else {
		    push( @stack, [] );
		}
	    }
	}
	else {
	    die( "Syntax error: unknown syntax element ", &$place(), ".\n" );
	}
    }
    @stack;
}

1;

=head1 AUTHOR

=over 4

=item
TSUCHIYA Masatoshi <tsuchiya@pine.kuee.kyoto-u.ac.jp>

=back

=head1 COPYRIGHT

利用及び再配布については GPL2 または Artistic License に従ってください。

=cut

__END__
# Local Variables:
# mode: perl
# coding: euc-japan
# use-kuten-for-period: nil
# use-touten-for-comma: nil
# End:
