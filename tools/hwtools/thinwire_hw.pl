#!/usr/bin/env perl
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

# Usage: thinwire_hw <victim> <breaklocks> <lock message>

use strict;
use Getopt::Long;
Getopt::Long::Configure qw(no_ignore_case);

use Socket;
use IPC::Open2;
use Sys::Hostname;
use POSIX ":sys_wait_h";
use POSIX qw(setsid);
use FindBin qw($Bin);
use lib "$Bin/../lib";
use lib "$Bin/../../lib";
use KConf;

sub success($){
  my $x = shift;
  if($ENV{HW_VERBOSE} > 0) {
    print $x;
  }
}

sub dbglog($){
  my $x = shift;
  if($ENV{HW_VERBOSE} >= 3) {
    print $x;
  }
}


# We can't use "getlogin" because it is broken on AIX
my $username = (split /\s/, `whoami`)[0];

my $victim = shift @ARGV;
my $breaklocks = shift @ARGV;
my $msg = join " ", @ARGV;

my $kserial;
my $kconf = KConf->new();
if (defined $ENV{$victim . "_kserial"}){
  $kserial = $ENV{$victim . "_kserial"};
} else {
  $kserial = $kconf->get_field($victim, "kserial");
}

my $machine = $kconf->get_field($victim, "machine");
if (!defined $machine) {
  $machine = $victim;
}

if (!defined $victim){
  die "Error! Action requires a victim (machine) name";
}

my $ipaddr = gethostbyname($kserial) || die "gethostbyname $!";

my $paddr   = sockaddr_in(4242, $ipaddr);
my $proto   = getprotobyname('tcp');
socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";

connect(SOCK, $paddr) || die "connect to $kserial: $!";

my $cmd = "thinver 3;" .
	  "owner $username;" .
	  "breaklocks $breaklocks;" .
	  "victim $machine;" .
	  "msg $msg;" .
	  "ktw; disconnect\n";

dbglog("Write to ktwd: $cmd\n");
syswrite(SOCK, $cmd);

my $line;
while(defined($line = <SOCK>)){
  dbglog $line;
  if($line=~/SUCCESS/){
    success "OK: acquired victim lock\n";
    exit 0;
  }
}
print "Failed to get lock\n";

exit -1;
