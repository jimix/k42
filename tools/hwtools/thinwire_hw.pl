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

sub parseConfig($){
  my $victim = shift;
  my $hwcfg;
  open CONFIG, "kvictim all $victim|"
    or die "FAIL: Failed to read config: $!\n";

  my $line;
  while($line = <CONFIG>){
    $line =~ /^(\S*) (.*)$/;
    $hwcfg->{$1} = $2;
  }
  $hwcfg->{machine}=$victim;
  close CONFIG;
  return $hwcfg;
}

# We can't use "getlogin" because it is broken on AIX
my $username = (split /\s/, `whoami`)[0];

my $victim = shift @ARGV;
my $breaklocks = shift @ARGV;
my $msg = join " ", @ARGV;

my $hwcfg = parseConfig($victim);

if (!defined $victim){
  die "Error! Action requires a victim (machine) name";
}

my $ipaddr = gethostbyname($hwcfg->{kserial}) || 
  die "gethostbyname $!";

my $paddr   = sockaddr_in(4242, $ipaddr);
my $proto   = getprotobyname('tcp');
socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";

connect(SOCK, $paddr) || die "connect to $hwcfg->{kserial}: $!";

my $cmd = "thinver 3;ktw $username $victim $breaklocks $msg; disconnect\n";

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
