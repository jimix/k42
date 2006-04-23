#!/usr/bin/env perl
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

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

my $verbose = 0;
my $imgver = 0;
my $site;
my $machine;
my $reboot;
my $poweroff;
my $breaklocks;
my $timeout;
my $hwcfg;
my $printenv;
my $kconf = KConf->new();

sub success($){
  my $x = shift;
  if($verbose>0) {
    print $x;
  }
}
sub dbglog($){
  my $x = shift;
  if($verbose>1) {
    print $x;
  }
}

# We can't use "getlogin" because it is broken on AIX
my $username = (split /\s/, `whoami`)[0];

# Put ourselves in the kill_pids list
# Any fork/exec failure should kill everything, thus
# after any fork, this process should be in the child's kill_pids
my %kill_pids;
my $self_kill = { 'sig' => 'TERM', 'pid' => -$$, 'cmd' => 'hwconsole'};
$kill_pids{$$} = $self_kill;

$SIG{INT} = 'do_killall';
$SIG{TERM} = 'do_killall';

setpgrp;

########################################################################
#
# Kill all processes in the kill_pids list
#
sub do_killall($$){
  my $sig = shift;
  dbglog "Running $sig handler in $$\n";
  $SIG{TERM} = 'IGNORE';
  foreach my $p (keys %kill_pids){
    my $x = $kill_pids{$p};
    my @y = split /\s+/, $x->{cmd};
    dbglog "Kill $x->{pid} with $x->{sig} ($y[0])\n";
    kill $x->{sig}, $x->{pid};
  }

  # We are passed exit code if sig is 'EXIT'
  if($sig eq 'EXIT'){
    exit shift;
  }

  exit 0;
}





########################################################################
#
# Run a command, create a process descriptor and register it with
# kill_pids, so do_killall can clean-up
#
sub run_command($$){
  my $cmd = shift;
  my $newgrp = shift;
  my $pid = fork();

  if($pid == 0){
    if($newgrp) {
      setpgrp;
    }	
    dbglog("+ $cmd\n");
    exec $cmd;
    dbglog "Failed exec: ($cmd) -$?\n";
    do_killall 'EXIT', -$?;
  }


  # If process group, kill's should kill the group
  if($newgrp){
    $pid = -$pid;
  }
  my $desc = { 'sig' => 'KILL', 'pid' => $pid, 'cmd' => $cmd };
  $kill_pids{abs $pid} = $desc;
  return $desc;

}


########################################################################
#
# Run a command, wait for it to complete
#
sub run_command_wait($){
  my $cmd = shift;
  my $desc = run_command($cmd, 1);

  my $ret = waitProcess(abs $desc->{pid});
  if($ret == 0) {
    success "OK: $cmd\n";
  } else {
    if($ret > 256){
      $ret = $ret >> 8;
    }
    dbglog "FAIL: '$cmd' returned: $ret\n";
  }
  return $ret;
}

##############################################################################
#
# Wait for a process to finish, returns its exit code
#
sub waitProcess($) {
  my $pid = shift;
  my $s;
  my $p;
  do {
    $p = waitpid(-1, 0);
    $s = $?;
    my $desc = $kill_pids{$p};
    my $cmd = 'UNKNOWN';
    if($desc->{cmd}){
      $cmd = $desc->{cmd};
    }
    dbglog "wait returned $p: process: ($cmd) $s\n";
  } until($p == abs $pid);
  return $s;
}



##############################################################################
#
# Read from kconf
#
#
sub parseConfig($){
  my $victim = shift;
  my $hwcfg = $kconf->flatten($victim);
  if (!defined $hwcfg) {
    die "Can't get config for $victim\n";
  }
  foreach my $k (keys %{$hwcfg}) {
    my $env_name = $victim . "_" . $k;
    if (defined $ENV{$env_name}) {
      $hwcfg->{$k} = $ENV{$env_name};
    } else {
      $ENV{$env_name} = $hwcfg->{$k};
    }
  }
  # The hw config may be an "alias" of a machine, in which case it
  # may specify "machine" explicitly.
  if (!defined $hwcfg->{machine}) {
    $hwcfg->{machine}=$victim;
  }
  return $hwcfg;
}

##############################################################################
#
# Talk to ktwd and get console lock, do no start ktwd
#
#
sub lockOnlyConsole($$){
  my $breaklocks = shift;
  my $msg = shift;
  my $ipaddr = gethostbyname($hwcfg->{kserial}) || 
    die "ERROR: gethostbyname $!";

  my $paddr   = sockaddr_in(4242, $ipaddr);
  my $proto   = getprotobyname('tcp');
  socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";

  connect(SOCK, $paddr) || die "FAIL: connect to $hwcfg->{kserial}: $!";
  if(defined $breaklocks){
    $breaklocks = "breaklock " . $breaklocks . ";";
  }

  if(defined $msg && $msg ne ""){
    $msg = "msg " . $msg . ";";
  }
  syswrite(SOCK, "owner $username; victim $hwcfg->{machine}; " .
	   $msg . " " . $breaklocks . " lock; disconnect\n");
  my $line;
  while(defined($line = <SOCK>)){
    dbglog "lock: $line";
    if($line=~/SUCCESS/){
      success "OK: locked console\n";
      return 1;
    }
  }
  return 0;
}

##############################################################################
#
# Talk to ktwd and release console lock, start ktwd
#
#
sub release($){
  my $victim = shift;

  my $ipaddr = gethostbyname($hwcfg->{kserial}) || 
    die "gethostbyname $!";

  my $paddr   = sockaddr_in(4242, $ipaddr);
  my $proto   = getprotobyname('tcp');
  socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";

  connect(SOCK, $paddr) || die "FAIL: connect to $hwcfg->{kserial}: $!";

  syswrite(SOCK, "owner $username; " .
	   "victim $victim; release; disconnect\n");
  my $line;
  while(defined($line = <SOCK>)){
    dbglog "lock release: $line";
  }
  return 0;
}

sub poweroff($){
  my $hwcfg = shift;
  if($hwcfg->{poweroff} eq ""){
      print "FAIL: do not know how to power off machine `$hwcfg->{machine}'\n";
      return 1;
  }

  my $desc = run_command("$hwcfg->{poweroff} $hwcfg->{machine}",1);

  my $ret = waitProcess(abs $desc->{pid});
  if($ret == 0) {
    success "OK: poweroff\n";
  } else {
    if($ret > 256){
      $ret = $ret >> 8;
    }
    dbglog "FAIL: poweroff script returned: $ret\n";
  }
  return $ret;
}

##############################################################################
#
# Talk to ktwd to get status
#
#
sub status(){
  my $vic;
  my @vics = $kconf->match_key("victim_type", "hw");
  my %hosts;
  foreach my $v (@vics){
    my $x = $kconf->flatten($v);
    $hosts{$x->{kserial}} = 1;
  }

  foreach my $h (keys %hosts){
    my $ipaddr = gethostbyname($h) || next;
    my $paddr   = sockaddr_in(4242, $ipaddr);
    my $proto   = getprotobyname('tcp');

    socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";
    connect(SOCK, $paddr) || die "connect to $h: $!";
    syswrite(SOCK, "status; disconnect\n");
    my $line;
    while(defined($line = <SOCK>)){
      print $line;
    }
  }
  return 0;
}

sub single_status($){
  my $hwcfg = shift;

  my $ipaddr = gethostbyname($hwcfg->{kserial}) || next;
  my $paddr   = sockaddr_in(4242, $ipaddr);
  my $proto   = getprotobyname('tcp');

  socket(SOCK, PF_INET, SOCK_STREAM, $proto)	|| die "socket: $!";
  connect(SOCK, $paddr) || die "connect to $hwcfg->{kserial}: $!";
  syswrite(SOCK, "status $hwcfg->{machine}; disconnect\n");
  my $line;
  while(defined($line = <SOCK>)){
    print $line;
  }
  return 0;
}


##############################################################################
#
# Reboot the specified victim
#
#
sub reboot($){
  my $hwcfg = shift;
  if($hwcfg->{reboot} eq ""){
      # Set errno to ENOSYS on a Linux system.
      $!=38;
      print "FAIL: do not know how to reboot machine `$hwcfg->{machine}'\n";
      return 1;
  }

  my $desc = run_command("$hwcfg->{reboot} $hwcfg->{machine}",1);

  my $ret = waitProcess(abs $desc->{pid});
  if($ret == 0) {
    success "OK: reboot\n";
  } else {
    if($ret > 256){
      $ret = $ret >> 8;
    }
    dbglog "FAIL: Reboot script returned: $ret\n";
  }
  return $ret;
}

#############################################################################
#
# Die
#
sub abort($$){
  my $msg = shift;
  my $code = shift;
  usage();
  print "\nhwconsole: ERROR: $msg";
  exit $code;
}

sub usage() {
print <<EOF;
'hwconsole' helps you remote control a victim machine

Usage: hwconsole [OPTION [ARGS]] ...
 -h, --help          Show this usage statement
 -v, --verbose L     Be verbose at level L (1 is status, 2 is debug)
 -m, --machine M     Select victim machine M for operations
 -s, --status        Show status of victim machine(s) and exit
 -e, --environment   Print environment variables before starting operations
 -f, --file F        Use boot image file F
 -B, --breaklocks K  Break locks with action K (kill, steal, or killsteal)
 -R, --reboot [A]    Reboot victim (if optional arg A is 'only', stop after)
 -p, --poweroff      Power off victim
 -n, --nosimip       Do not start SimIP
 -t, --timeout N     Kill thinwire and exit after N seconds
 -M, --message S     Put message S in lock file
 -g, --get           Acquire lock on victim
 -x, --release       Release lock on victim

Notes:
 If victim machine M is 'mambo', a Mambo simulator instance will be used.
 Any argument following '--' is treated as a NAME=VALUE environment pair.

Examples:
 hwconsole -s
 hwconsole -m foo -R only
 hwconsole -m foo -R -B killsteal -f bar
 hwconsole -R -- HW_VICTIM=foo HW_BOOT_FILE=bar HW_NO_SIMIP=1
 hwconsole -m foo -x
EOF
}



my $help;
my $release;
my $status;
my $acquire;
my $lockMsg;
my $bootfile;
my $nosimip;
GetOptions("B|breaklocks=s" => \$breaklocks,
	   "R|reboot:s" => \$reboot,
	   "p|poweroff" => \$poweroff,
	   "g|get" => \$acquire,
	   "f|file=s" => \$bootfile,
	   "x|release" => \$release,
	   "v|verbose=i" => \$verbose,
	   "M|message=s" => \$lockMsg,
	   "t|timeout=i" => \$timeout,
	   "m|machine=s" => \$machine,
	   "s|status" => \$status,
	   "n|nosimip" => \$nosimip,
	   "h|help"	=> \$help,
	   "e|environment" => \$printenv);

# Anything following "--" is parsed as "NAME=VALUE" and inserted into env
while($#ARGV!=-1){
  my $arg = $ARGV[0];
  if($arg=~/^([^=]+)=(.*)$/){
    $ENV{$1}=$2;
  }
  shift @ARGV;
}

if(defined $help){
  usage;
  exit;
}

# Associate command line args with env var names
my @bindings = ( \$machine,'HW_VICTIM',
		 \$bootfile, 'HW_BOOT_FILE',
		 \$verbose, 'HW_VERBOSE',
		 \$nosimip,'HW_NO_SIMIP');

# If cmd line arg not set, get definition from env var
while($#bindings != -1){
  my $var = shift @bindings;
  my $name= shift @bindings;
  if(!defined $$var && defined $ENV{$name}){
    $$var = $ENV{$name};
  } elsif(defined $$var){
    $ENV{$name} = $$var;
  }
}

# Get the name of our site
my $s = $kconf->flatten("site");
$site = $s->{name};
$ENV{site_name} = $site;

if($status==1 && !defined $machine){
  status;
  exit 0;
}


$hwcfg = parseConfig($machine);

if($hwcfg->{victim_type} ne "hw"){
  if(defined $reboot || defined $release ||
     defined $breaklocks || defined $acquire || defined $poweroff){
    print "ignoring reboot, poweroff and locking operations on mambo\n";
    undef $reboot;
    undef $release;
    undef $poweroff;
    undef $breaklocks;
    undef $acquire;
  }
}

if($status==1){
  single_status $hwcfg;
  exit 0;
}


if(!defined $hwcfg){
  die "$machine is not a valid victim\n";
}

if(defined $reboot && $reboot eq "only"){
  print "Reboot: $reboot $machine\n";
  exit reboot($hwcfg);
}

if ($poweroff==1){
  poweroff($hwcfg);
  if (!($release == 1)) {
    exit 0;
  }
}

if($release==1){
  release($machine);
  exit 0;
}

if(defined $breaklocks && $breaklocks ne "kill" 
   && $breaklocks ne "steal" && $breaklocks ne "killsteal"){
  abort "Bad 'breaklocks' argument\n", 1;
}

if($acquire==1) {
  lockOnlyConsole($breaklocks, $lockMsg);
  exit 0;
}

if(defined $printenv){
  foreach my $x (keys %ENV) {
    print "$x\t $ENV{$x}\n";
  }

}


my $waitfor;

if(!defined $bootfile){
  abort "please supply your desired boot image with `--file'\n", 1;
}




my $tw_port;

if ($hwcfg->{victim_type} eq "hw") {
  if(!lockOnlyConsole($breaklocks, $lockMsg)) {
    die "Failed to lock victim\n";
  }
}

if (defined $hwcfg->{kinstall}) {
  if (run_command_wait("$hwcfg->{kinstall} $bootfile $machine") != 0) {
    die "Kernel installation failure: $? $!\n";
  }
}





if(defined $reboot){
  my $ret = reboot($hwcfg);
  if($ret!=0){
    die "FAIL: reboot script returned: $ret ($!)\n";
  }
  # Wait for the machine to actually power-cycle
  #    sleep 5;
}

if(defined $hwcfg->{async_cmd}){
  my @cmds = split /,/, $hwcfg->{async_cmd};
  foreach my $cmd (@cmds) {
    chomp $cmd;
    my @words = split /\s+/, $cmd;
    next if($#words < 0);
    if ($#words == 1) {
      run_command("$cmd $machine", 1);
    } else {
      run_command("$cmd", 1);
    }
  }
}

sleep 1;



if (defined $hwcfg->{sync_cmd}) {
  my @cmds = split /,\s*/, $hwcfg->{sync_cmd};
  foreach my $cmd (@cmds) {
    chomp $cmd;
    my @words = split /\s+/, $cmd;
    next if($#words < 0);
    print "run command: $#words\n";
    if ($#words == 0) {
      $cmd = "$cmd $machine $breaklocks $lockMsg";
    }
    if(run_command_wait($cmd) != 0) {
      die "FAIL: command '$cmd'\n";
    }
  }
}

sleep 1;

my $fg_cmd = "$hwcfg->{fg_cmd} $machine";
$waitfor = run_command($fg_cmd,0);

$SIG{INT} = 'IGNORE';  # We don't want ctrl-c



my $waitarg = -1;
my $p;
if(defined $timeout && $timeout>0){
  $SIG{ALRM} = 'do_killall';
  alarm $timeout;
}



waitProcess($waitfor->{pid});
print "complete\n";
do_killall 'EXIT', 0;
