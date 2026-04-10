#!/usr/bin/perl -w
use strict;
use warnings;
use Getopt::Std;
use WWW::Mechanize;
use Net::FTP;
use POSIX qw(strftime);

my %arg;
getopts("hu:P:p:r:n:f:",\%arg);

my $user = $arg{"u"} || $ENV{"USER"};
my $password = $arg{"P"} || "";
my $project_name = $arg{"p"};
my $package_name = $arg{"n"};
my $release_name = $arg{"r"};
my $file_name = $arg{"f"};

unless ($password) {
    open(F, ".passwd") || die "$!: .passwd\n";
    my $line = <F>;
    chomp $line;
    ($user, $password) = split /:/, $line;
    close(F);
}

sub usage {
    print "$0 -u user_name -P password -p project_name \n";
    print " -n package_name -r release_name -f file_name\n";
    exit;
}

usage () if ($arg{"h"} ||
	     !$project_name ||
	     !$package_name ||
	     !$release_name ||
	     !$file_name ||
             ! -f $file_name);

my $ftp = Net::FTP->new("upload.sourceforge.net", Debug => 1);
$ftp->login("anonymous",'anonymous@gmail.com');
$ftp->cwd("/incoming");
$ftp->binary();
$ftp->put("$file_name");
$ftp->quit;
sleep(20);

my $mech = new WWW::Mechanize(autocheck => 1);
print "logging in sourcefourge.net ...\n";
$mech->get("https://sourceforge.net/account/login.php");
$mech->form_number(2);
$mech->field('form_loginname' ,$user);
$mech->field('form_pw' ,$password);
$mech->click();

print "moving to projects/$project_name/admin ...\n";
$mech->get("/projects/$project_name/admin");
my @links = $mech->find_all_links(url_regex => qr[/project/admin/editpackages.php]);
my ($url, $title) = @{$links[0]};
print "moving to $url ...\n";
$mech->get($url);

my $str = $mech->content();
@links = $mech->find_all_links(url_regex => qr[newrelease.php]);
my $pid = -1;
while (1) {
    if ($str =~ s/<input type="text" name="package_name" value="([^\"]+)"//) {
	++$pid;
	last if ($1 eq $package_name);
    } else {
	last;
    }
}

($url, $title) = @{$links[$pid]};
print "moving $url\n";
print "uploading $file_name ...\n";
$mech->get($url);
$mech->form_number(4);
$mech->field('release_name', $release_name);
$mech->click();
$mech->form_number(5);
$mech->tick('file_list[]', $file_name);
$mech->click();
$mech->form_number(4);
$mech->field('release_date',
    strftime("%Y-%m-%d", localtime((stat($file_name))[9])));
$mech->click();
print "done\n";
