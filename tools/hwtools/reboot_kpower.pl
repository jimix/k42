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
my $retries = 0;

# How many times to retry on kpower errors
if (exists($ENV{HW_RETRIES})) {
    $retries =  $ENV{HW_RETRIES};
} else {
    $retries = 3;
}

sub dbglog($){
  my $x = shift;
  if($verbose) {
    print STDERR $x;
  }
}

my $victim = $ARGV[0];

my $cmd = "kvictim $victim kpower outlet";
dbglog("++ $cmd\n");
my @output = split '\s+', `$cmd`;

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

my @kpowerTalk = ({ 'read'=> "User Name :", 'write'=> "k42"},
		  { 'read'=> "Password  :", 'write'=> "k42"},
		  { 'read'=> "\n>", 'write'=> "1"},
		  { 'read'=> "\n>", 
		    'write'=> $port},
		  { 'read'=> "\n>", 'write'=> "3"},
		  { 'read'=> "Enter 'YES' to continue or <ENTER> to cancel :",
		    'write'=> "YES"},
		  { 'read'=> "Command successfully issued." },
		  { 'read'=> "Press <ENTER> to continue...", 'write'=> ""}
		 );
my @kpower2Talk = ({ 'read'=> 'Enter Selection>', 'write'=>"1"}, 
		   { 'read'=> 'RPC-3>', 
		     'write'=>'reboot ' . $port},
		   { 'read'=>'\? \(Y\/N\)>', 'write'=>"Y" },
		   { 'read'=> 'RPC-3>' });

my @kpower3Talk = ({ 'read'=> 'Enter user name:', 'write'=>"k42"}, 
		   { 'read'=> 'Enter Password:', 'write'=>"k42"}, 
		   { 'read'=> 'RPC3-NC>', 'write'=>'reboot ' . $port},
		   { 'read'=>'\(Y\/N\)\?', 'write'=>"Y" },
		   { 'read'=> 'RPC3-NC>' });

my @kpower4Talk = ({ 'read'=> 'RPC3-NC>', 'write'=>'reboot ' . $port},
		   { 'read'=>'\(Y\/N\)\?', 'write'=>"Y" },
		   { 'read'=> 'RPC3-NC>' });

my @bladepower = ({ 'read'=> 'username:', 'write'=>"k42"}, 
		   { 'read'=> 'password:', 'write'=>"k42k42"}, 
		   { 'read'=> 'system.*>', 
#		     'write'=>"sol -T system:blade[" . $port . "] -off"},
#		   { 'read'=> 'system.*>', 
#		     'write'=>"power -T system:blade[" . $port . "] -off"},
#		   { 'read'=> 'system.*>', 
#		     'write'=>"sol -T system:blade[" . $port . "] -on"},
#		   { 'read'=> 'system.*>', 
		     'write'=>"power -T system:blade[" . $port . "] -cycle"},
#		   { 'read'=> 'system.*>', 
#		     'write'=>"reset -T system:blade[" . $port . "]:sp"},
#		   { 'read'=> 'system.*>', 
#		     'write'=>"power -T system:blade[" . $port . "] -on"},
		   { 'read'=> 'OK' });


my $talk = {'kpower' => \@kpowerTalk,
	    'kpower2' => \@kpower2Talk,
	    'kpower3' => \@kpower3Talk,
	    'kpower4' => \@kpower4Talk,
	    'kblade' => \@bladepower};

# We step through the conversation sequence defined in the talk array for
# the selected programmable power supply.  If at any point we encounter an
# error, we start the whole sequence over again unless we are out of retries.
RESET: for ($i = 1; ; $i++){
    $t->open($kpower);
    $t->errmode("return");

    foreach my $z (@{$talk->{$kpower}}) {
	dbglog "Read: '$z->{read}'\nReply: '" . $z->{write} . "'\n";
	my @ret = $t->waitfor(Match => '/' . $z->{read} . '/', 
			      Timeout => 20);
	if($#ret<0){
	    print STDERR "Got error: " . $t->errmsg() . 
		" while trying to read '$z->{read}'\n";

	    if ($i > $retries) {
		print STDERR "giving up after $i attempts!\n";
		exit 1;
	    }
	    print STDERR "retrying $i of $retries times ...\n";
	    next RESET;
	}
	$t->print($z->{write});
    }

    $t->close;
    exit 0;
}
