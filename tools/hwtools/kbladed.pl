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
use IPC::Open2;
use Socket;
use POSIX ":sys_wait_h";
use POSIX qw(setsid);
$ENV{PATH} = "/home/thinwire/bin:$ENV{PATH}";

#$| = 1;
#open LOG, ">>/home/thinwire/kbladed.log";

my $verbose = $ENV{HW_VERBOSE};
$verbose =1;
sub dbglog($){
  my $x = shift;
  if($verbose) {
    print LOG $x;
  }
}

sub telnet_connect($$){
  my $kpower = shift;
  my $port = shift;

  my $t = new Net::Telnet(Binmode => 1,
			  Output_record_separator => "\r\n");
  my $pre;
  my $match;
  my $x;

  my $fh;
  my ($name,$aliases,$addrtype,$length,@addrs) = gethostbyname $kpower;
  dbglog "Telnet to " . inet_ntoa($addrs[0]) . "\n";
  my $paddr = sockaddr_in(23, $addrs[0]);
  socket($fh, PF_INET, SOCK_STREAM, getprotobyname('tcp')) or die "socket: $!";
  connect($fh, $paddr) or die "connect: $!";

  $t->fhopen($fh);
  $t->errmode("die");
  $t->login(Name=> "k42", Password=>"k42k42");

  foreach my $z (@bladepower) {
    dbglog "Read: '$z->{read}'\nReply: '" . $z->{write} . "'\n";
    my @ret = $t->waitfor(Match => '/' . $z->{read} . '/', 
			  Timeout => 15);
    if($#ret<0){
      dbglog STDERR "Got error: " . $t->errmsg() . 
	" while trying to read '$z->{read}\n";
      exit 1;
    }
    if(defined $z->{write}){
      $t->print($z->{write});
    }
  }

#  syswrite $t, "\xff\xfd\x00\xff\xfb\x00\xff\xfe\xff", 9;

  $t->print("console -T system:blade[" . $port . "] -o");

  my @ret = ($fh, $fh);
  return @ret;
}

sub ssh_connect($$){
  my $host = shift;
  my $port = shift;
  my @ret;

  my $pid = open2($ret[0], $ret[1], "ssh", "-T", 'k42@' . $host);
  dbglog("pid $pid fd:" . fileno($ret[0]) . " " . fileno($ret[1]) . "\n");

  sleep 10;
  print { $ret[1] } "console -T system:blade[$port] -o\n";
  dbglog "wrote string... $p $ret[0] $ret[1]\n";


  return @ret;
}




sub transfer(@){
  my @list = @_;
  my $bits;
  my $i = 0;
  $bits = '';
  for(; $i <= $#list ; ++$i) {
    vec($bits, fileno($list[$i]->{input}),1) = 1;
  }

  while(1){
    my $tmp = $bits;
    my $num;
    my $timeleft;

    ($num, $timeleft) = select($tmp, undef, undef, undef);
    $i = 0;
    while($i < 2){
      if(vec($tmp, fileno($list[$i]->{input}),1)){
	my $line;
	my $bytes = sysread $list[$i]->{input}, $line, 8;
	if($bytes>0){
	  syswrite $list[$i]->{output}, $line, $bytes;
	} else {
	  #print LOG "bytes: $list[$i]->{inname} to $list[$i]->{outname} $bytes\n";
	  exit 0;
	}
      }
      $i++;
    }
  }
}


my $victim;
if($ARGV[0] eq "inetd") {
  $victim = $ARGV[1];
}else{
  $victim = $ENV{HW_VICTIM};
}

if($victim eq "") { die "No victim specified\n" };

my @output = split '\s+', `kvictim $victim kpower outlet`;

my $kpower = $output[1];
my $port = $output[2];


my @pipe;

if(1) {
  @pipe = telnet_connect($kpower, $port);
}else {
  @pipe = ssh_connect($kpower, $port);
}


my @list;
my $x = { 'input' => STDIN, 'output' => $pipe[1], 
	  'inname' => 'stdin', 'outname' => 'pipe1' };
my $y = { 'input' => $pipe[0], 'output' => STDOUT,
	  'inname' => 'pipe0', 'outname' => 'stdout'};
push @list, $x;
push @list, $y;

transfer(@list);

exit 0;



