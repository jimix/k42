#!/usr/bin/perl -w

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: listen_server.pl,v 1.3 2003/08/08 15:19:57 kdiaa Exp $
# ############################################################################


use POSIX ":sys_wait_h";

use strict;
use IO::Socket ();
use Getopt::Long ();
use vars qw($debug $verbose $PORT $TOHOST $TOPORT $DIR);
$PORT = 8181;
$TOHOST = "127.0.0.1";
$TOPORT = 80;
$DIR = undef;
$| = 1;
############################################################################
#
#This is main()
#
############################################################################
{
  my %o = ('port' => $PORT,
	   'toport' => $TOPORT,
	   'tohost' => $TOHOST);
  Getopt::Long::GetOptions(\%o, 'debug', 'verbose+', 'port=s', 'toport=s',
			   'tohost', 'dir=s');
  $verbose = 1 if $debug && !$verbose;
  my $ah = IO::Socket::INET->new('LocalAddr' => "0.0.0.0",
				 'LocalPort' => $PORT,
				 'Reuse' => 1,
				 'Listen' => 10)
    || die "Failed to bind to local socket: $!";
  print "Entering main loop.\n" if $o{'verbose'};
  $SIG{'CHLD'} = 'IGNORE';
  my $num = 0;
  while (1) {
    my $ch = $ah->accept();
    if (!$ch) {
      print STDERR "Failed to accept: $!\n";
      next;
    }
    printf("Accepting client from %s, port %s.\n",
	   $ch->peerhost(), $ch->peerport()) if $o{'verbose'};
    ++$num;
    my $pid = fork();
    if (!defined($pid)) {
      print STDERR "Failed to fork: $!\n";
    } elsif ($pid == 0) {
      # This is the child
      $ah->close();
      Run(\%o, $ch, $num);
    } else {

      my $result = waitpid($pid, 0);

      print "Parent: Forked child, closing socket ($result).\n" if $o{'verbose'};
      $ch->close();
    }
  }
}




sub Run {
  my($o, $ch, $num) = @_;
  my $fh;
  if (!$o->{'dir'}) {
    $o->{'dir'}=".";
  }
  $fh = Symbol::gensym();
  open($fh, ">$o->{'dir'}/connection$num.log")
    or die "Child: Failed to create file $o->{'dir'}/tunnel$num.log: $!";

  $ch->autoflush();
  while ($ch ) {
    print "Child: Starting loop.\n" if $o->{'verbose'};
    my $rin = "";
    vec($rin, fileno($ch), 1) = 1 if $ch;
	print "Child: 1.\n";
    my($rout, $eout);
    select($rout = $rin, undef, $eout = $rin, 120);
    print "Child: 2.\n";
    if (!$rout && !$eout) {
      print STDERR "Child: Timeout, terminating.\n";
    }
    my $cbuffer = "";
    my $tbuffer = "";
    print "Child: 3.\n";
    if ($ch && (vec($eout, fileno($ch), 1) ||
		vec($rout, fileno($ch), 1))) {
      print "Child: Waiting for client input.\n" if $o->{'verbose'};
      my $result = sysread($ch, $tbuffer, 1024);
      if (!defined($result)) {
	print STDERR "Child: Error while reading from client: $!\n";
	exit 0;
      }
      if ($result == 0) {
	print "Child: Client has terminated.\n" if $o->{'verbose'};
	exit 0;
      }
      if ($fh && $tbuffer) {
	(print $fh $tbuffer);
      }
      print "Child: Client input: $tbuffer\n" if $o->{'verbose'};

      if ( $tbuffer =~ /EOF:/i) {
	print "client requested connection close \n";
	exit(0);
      }
      elsif ( $tbuffer =~ /reboot:/i) {
	  $tbuffer =~ s/reboot://i;
	  exec "$o->{'dir'}/reboot.sh $tbuffer" or die "Child: Failed to execute reboot: $!";
      }
      elsif ( $tbuffer =~ /doprofile:/i) {
	  $tbuffer =~ s/doprofile://i;
	  exec "$o->{'dir'}/doprofile.sh $tbuffer" or die "Child: Failed to execute doprofile: $!";
      }
      elsif ( $tbuffer =~ /([^\s:]+):/i) {
	print "Run command: $1\n";
	close($ch);
	exec "$o->{'dir'}/$1.sh" or 
	  die "Child: Failed to execute setup: $!";
      } else{
	die "Child: Unknown command\n";
      }
    }
  }
}


