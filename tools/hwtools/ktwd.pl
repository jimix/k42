#!/usr/bin/perl
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

#
# Install with inetd/xinetd, listen on port 4242
#


use strict;
use File::stat;
use File::Basename;
use Fcntl ;
use Sys::Hostname;
use Socket;
use POSIX ":sys_wait_h";
use POSIX qw(strftime);
use Config;


my $basedir;
BEGIN{
  use Cwd 'abs_path';
  $basedir = abs_path(dirname($0)) . '/../';
}

$ENV{PATH} = $ENV{PATH} . ":$basedir/bin";
my $lockdir = "$basedir/lock";
my $ktwdlog = "/dev/null";


my %signo;
my @signame;
defined $Config{sig_name} || die "No sigs?";
my $i=0;
foreach my $name (split(' ', $Config{sig_name})) {
  $signo{$name} = $i;
  $signame[$i] = $name;
  $i++;
}

my $hwcfg;

my $owner;
my $logname = $ktwdlog;
my $breaklock;
my $msg;
my $disconnect;
my $ktwpid;
my $thinver = "2";


open(LOG,">>$ktwdlog");

sub timeStamp(){
  return strftime( "[%T %D]", localtime(time));
}

sub output($){
  my $line = shift;
  my $string = timeStamp() . ": $line";
  print $line;
  print LOG $string;
}


sub lockfile($){
  my $victim =shift;
  return "$lockdir/lock_${victim}";
}

sub parseConfig($){
  my $victim = shift;

  open CONFIG, "kvictim $victim kserial ktw " .
	       "TW_BASE_PORT serial1_speed serial2_speed| ";

  my $line = <CONFIG>;

  close CONFIG;

  if($line eq ""){
    return undef;
  }

  (my $machine, my $conshost, my $ktw, my $port, my $s1, my $s2) = 
    split /\s+/, $line;

  my $cfg;

  $cfg->{machines} = $machine;
  $cfg->{kserial} = $conshost;
  $cfg->{ktwtty} = $ktw;
  $cfg->{portbase} = $port;

  if($s1 ne "#") {
    $cfg->{speed1} = $s1;
  }
  if($s2 ne "#") {
    $cfg->{speed2} = $s2;
  }
  return $cfg;
}



$|=1; #autoflush

sub getStatus(\@){
  my $targets = shift;
  my @name = split /\./, hostname();
  my %thash;
  foreach my $x (@{$targets}) {
    $thash{$x} = 1;
  }
  my @vics = split /\s+/,  `kvictim kserial=$name[0]`;
  foreach my $x (@vics) {
    next if($thash{$x} != 1);

    my @words;
    my $pid;
    my $user;
    my $line;
    if(open(LOCKFILE, "<" . lockfile($x))) {
      $line = <LOCKFILE>;
      @words = split /\s+/, $line;
      close(LOCKFILE);
    }
    $pid = $words[0];
    $user = $words[1];
    shift @words;
    shift @words;

    my $extra;
    if($pid && ! -d "/proc/$pid"){
      $pid = "";
    }
    output sprintf("%8.8s $x\t$pid\t$user " . 
		   join(" ",@words) . "\n", $name[0]);
  }
}

sub readLockFile($){
  my $victim = shift;
  open(LOCKFILE, "<" . lockfile($victim)) || return;
  my $line = <LOCKFILE>;
  close(LOCKFILE);
  return split /\s+/, $line;
}

sub getLockFile($$$$$){
  my $victim = shift;
  my $newpid = shift;
  my $owner = shift;
  my $breaklock = shift;
  my $message = shift;
  my $retry = 5;

  my $fname = lockfile($victim);
  while($retry>0){
    my $lockpid=-1;
    my $lockowner;
    # lock needs breaking/clenaup
    if(open(LOCKFILE, "<$fname")){
      my $line = <LOCKFILE>;
      if($line){
	($lockpid, $lockowner) = split /\s+/, $line;
      }
      close(LOCKFILE);
      output "Found $victim locked by $lockpid $lockowner\n";

      #
      # breaklock == 3 (killsteal), always grab lock
      # breaklock == 2 (steal), steal lock if not in use
      # breaklock == 1 (kill), kill thinwire, but no owner change
      # breaklock == 0 grab only if not in use
      #
      if ($lockowner eq $owner && $lockpid == 0) {
	  unlink $fname;
      } elsif((($lockowner eq $owner) && $breaklock==1) || $breaklock==3){
	if(($lockpid!=-1) && ($lockpid>0)) {
	  kill $signo{TERM}, $lockpid;
	  sleep 1;
	}
      }

      if($lockowner eq $owner || $breaklock >= 2){
	if((! (-d "/proc/$lockpid"))){
	  output "Removing lock " . (-d "/proc/$lockpid")  . "\n";
	  unlink $fname;
	}
      }
    }

    if(sysopen(LOCKFILE, $fname, O_TRUNC | O_WRONLY | O_CREAT | O_EXCL)){
      # got it!
      output "SUCCESS: Locked $fname for $newpid $owner $message\n";
      print LOCKFILE "$newpid $owner " . timeStamp() . " $message\n";
      close LOCKFILE;
      return 1;
    }

    if($breaklock){
      return 0;
    }
    --$retry;
  }

  return 0;
}

sub runktw($$$$$){
  my $victim = shift;
  my $owner  = shift;
  my $breaklock = shift;
  my $message = shift;
  my $thinver = shift;
  my $ktwtty = $hwcfg->{ktwtty};
  my $portbase = $hwcfg->{portbase};

  # Maintain a reference to socket fd
  open(SOCKET, ">&STDERR");



  #
  # Fork a process to run thinwire.
  # Parent then gets the ktw lock, while child waits
  # Once parent has the lock, child continues (lock identifies lock owner)
  #
  my $ktwlock = lockfile($victim);

  my @args;
  push @args, "thinwire" . $thinver;
  if(defined $hwcfg->{speed2} && defined $hwcfg->{speed1}){
    push @args, "-s";
    push @args, $hwcfg->{speed1};
    push @args, $hwcfg->{speed2};
  }
  push @args, $ktwtty;
  for(my $parts = 0; $parts < 8; ++$parts) {
    for(my $port = 0; $port < 5; ++$port){
      push @args, $portbase + $parts * 8 + $port;
    }
    push @args, ":";
  }

  my $ktwpid = fork();
  if($ktwpid == 0) {
    close(TTYIN);
    close(TTYOUT);
    while(1){
      if(!open(LOCKFILE, "<$ktwlock")){
	output "Child failed to open file: $ktwlock $!\n";
      }
      (my $pid, my $owner) = split /\s+/, <LOCKFILE>;
      close(LOCKFILE);
      if("$pid" eq "$$"){
	close(STDERR);
	close(STDOUT);
	close(STDIN);
	open(STDIN, "</dev/null");
	open(STDOUT, ">>$logname");
	open(STDERR, ">>$logname");
	
	exec @args;

	# Get here only on failure
	#close(STDERR);
	#open(STDERR, ">&SOCKET");
	die "FAILURE: thinwire exec failure: $?\n";
      }else{
	sleep 1;
      }
    }
    # never get here
  }

  if(!getLockFile($victim, $ktwpid, $owner, $breaklock, $message)) {
    kill 9, $ktwpid;
    die "FAILURE: Can't get ktw lock\n";
  }
  output "SUCCESS: acquired lock for $victim\n";
  return $ktwpid;
}


my $victim;

sub setBreakLock(@){
  my @words = @_;
  my $word = $words[1];
  if($word eq "kill"){
    $breaklock = 1;
  }elsif($word eq "steal"){
    $breaklock = 2;
  }elsif($word eq "killsteal"){
    $breaklock = 3;
  }elsif($word eq "none"){
    $breaklock = 0;
  }else{
    die "FAILURE: Bad breaklock command $word";
  }
}

sub getLock(@) {
  my @words = @_;
  if(!defined $owner){
    output "No owner defined\n";
  }elsif(!defined $victim){
    output "No vicitm defined\n";
  }else{
    getLockFile($victim, 0, $owner, $breaklock, $msg);
  }
}


sub setVictim(@) {
  my @words = @_;
  if ($#words>0) {
    my $vic = @words[1];
    my @name = split /\./, hostname();

    $hwcfg = parseConfig($vic);
    if(!defined $hwcfg){
      die "FAILURE: Unknown victim: $vic";
    }

    if($hwcfg->{kserial} ne $name[0]){
      die "FAILURE: Wrong host. Use $hwcfg->{kserial} " . 
	$name[0] . " for victim $vic";
    }
    $victim = $vic;
    output "Handling victim: $victim\n";
  }
}

sub setOwner(@) {
  my @words = @_;
  if ($#words>0){
    $owner = $words[1];
  }
}

sub setMsg(@) {
  my @words = @_;
  shift @words;
  $msg = join(' ', @words);
  output "Set msg: $msg\n";
}

my $settings = [ { 'name' => 'tw_args',
		   'action' => sub(@) {
		     my @words = @_;
		     print "Got tw_args: $words[0] $words[1]\n";
		     return 0;
		   }
		 },
		 { 'name' => 'breaklock',
		   'action' => *setBreakLock
		 },
		 { 'name' => 'lock',
		   'action' => *getLock
		 },
		 { 'name' => 'victim',
		   'action' => *setVictim
		 },
		 { 'name' => 'owner',
		   'action' => *setOwner
		 },
		 { 'name' => 'msg',
		   'action' => *setMsg
		 },
	       ];

# allow multiple commands per line, with ";"
READCONFIG:while(1){
  my $line = <STDIN>;
  if(!defined $line){
    exit 0;
  }

  my @cmds = split /;/, $line;

 CMDS:foreach my $cmd (@cmds) {

    $cmd=~s/^\s+(\S.*)$/$1/;
    my @words = split /\s+/, $cmd;

    foreach my $q (@{$settings}) {
      next if ($q->{name} ne $words[0]);
      $q->{action}(@words);
      next CMDS;
    }

    if($words[0] eq "ktw"){
      my $msg;
      # Use predefined victim/owner or expect them to follow
      if($#words >= 2){
	$owner = $words[1];
	my @r;
	push @r, "victim" , $words[2];
	setVictim(@r);

	if($#words > 2){
	  my @w;
	  push @w , "breaklock" , $words[3];
	  setBreakLock(@w);
	}
	if($#words > 3){
	  $msg = $words[4];
	}
      }	
      if(!defined $owner){
	output "No owner defined\n";
      }elsif(!defined $victim){
	output "No vicitm defined\n";
      }else{
	$ktwpid = runktw($victim, $owner, $breaklock, $msg, $thinver);
      }
    } elsif($words[0] eq "breaklock"){

      setBreakLock($words[1]);
    } elsif($words[0] eq "thinver"){

      $thinver = $words[1];

    } elsif($words[0] eq "disconnect") {

      exit;

    } elsif($words[0] eq "release") {

      if(!defined $victim){
	next;
      }
      if(!defined $owner){
	next;
      }
      my @l = readLockFile($victim);
      if($l[0]>0 && ($owner eq $l[1] || $breaklock==1 || $breaklock==3)){
	kill $signo{TERM}, $l[0];
      }
      if($owner eq $l[1] || $breaklock==2 || $breaklock==3){
	unlink lockfile($victim);
      }

    } elsif($words[0] eq "status") {

      my @k;
      if($#words == 0 || $words[1] eq "all") {
	my @name = split /\./, hostname();
	@k = sort(split '\n', `kvictim kserial=$name[0]`);
      } else {
	@k = ($words[1]);
      }
      getStatus(@k);

    }
  }
}

exit 0;
