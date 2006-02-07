#!/usr/bin/perl
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
use lib '/u/kitchawa/lib/perl';
use Net::Telnet;

my $verbose = $ENV{HW_VERBOSE};

sub dbglog($){
  my $x = shift;
  if($verbose) {
    print $x;
  }
}

my $victim = $ARGV[0];

my @output = split '\s+', `kvictim $victim kpower outlet`;

my $kpower = $output[1];
my $port = $output[2];

my $t;
$t = new Net::Telnet();
my $pre;
my $match;
my $x;

if($verbose>0){
  print "Contacting $kpower to reboot $victim\n";
}




$t->open($kpower);
$t->errmode("die");
foreach my $z (@{$talk->{$kpower}}) {
  dbglog "Read: '$z->{read}'\nReply: '" . $z->{write} . "'\n";
  my @ret = $t->waitfor(Match => '/' . $z->{read} . '/', 
			Timeout => 15);
  if($#ret<0){
    print STDERR "Got error: " . $t->errmsg() . 
      " while trying to read '$z->{read}\n";
    exit 1;
  }
  $t->print($z->{write});
}
$t->close;
exit 0;
