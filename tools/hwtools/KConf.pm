#!/usr/bin/perl
#############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: KConf.pm,v 1.2 2006/01/18 22:01:33 mostrows Exp $
# ############################################################################
package KConf;
use POSIX;
use strict;
use IO::Socket::INET;
use IO::Handle;
use IO::File;
use FindBin qw($Bin);
use Cwd qw(abs_path);

#
# Each victim can have any arbitrary (field,value) pair, as necessary
# to support the tools and environment for that victim machine.  Here
# are some of the fields used by the ktwd/thinwire/hwconsole tools.
# Where indicated, some fields are used only by site-specific tools,
# and may not be applicable for other victims/sites
#

# A victim may specify an "inherit" field, which identifies a parent
# set of field values that are searched if the field being sought does
# not exist in the victims set of field values.
#
# Multiple inherit field values are seperated by commas.
#
#
# For an inheritance tree such as this:
#
# (A has:  inherit => 'B,C'
#
#             A
#            / \
#           B   C
#          / \ / \
#          D E F G
#
# The search order is: A B C D E F G
# Once a field value is found, any subsequent definitions are ignored
#

# Any key (victim name) that begins with "_" is not considered a valid
# victim, but rather as a set of fields and values that can be
# inherited as a group by a victim.



# This inheritance mechanism allows one to define a set of common
# values (perhaps site-specific values or machine-type specific
# values) that can be easily associated with victims.


BEGIN {
  use Exporter();
  our($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
  $VERSION = sprintf "%d.%03d", q$Revision: 1.2 $ =~ /(\d+)/g;
  @ISA = qw(Exporter);
  @EXPORT = qw(new print_all, match_key, read_conf_files, flatten);
  @EXPORT_OK = qw(@install_dir);
}

our @EXPORT_OK;

our @install_dir;

@install_dir = ();

sub new () {}

sub flatten($$) {}
sub match_key($$$) {}
sub print_all($$@) {}
sub get_key($$) {}
sub get_field($$$) {}

#
# $confs - ref to array containing conf files to read
# $func  - function to call for each line
# @func_args - args to function
#
sub read_conf_files($$@) {}


END {}

my $do_debug = 0;
sub debug($) {
  my $x = shift;
  if ($do_debug == 0) { return; }
  print "# $x";
}

#
# Return a flat hash of all values for a given victim name
#
sub flatten($$){
  my $self = shift;
  my $start = shift;
  #
  # Print out  field, value pairs,  for each field defined
  # for the specified victim.
  #
  my @next;
  push @next, $start;

  my $vals;
  my $sources;
  do {
    $start = shift @next;
    foreach my $field (keys %{$self->{victims}->{$start}}) {
      if($field eq "inherit"){
	# Remember inheritance keys to look up later
	push @next, split(/,/, $self->{victims}->{$start}->{$field});
      } else {
	if(!defined $vals->{$field}){
	  $vals->{$field} = $self->{victims}->{$start}->{$field};
	  $sources->{$field} = $self->{sources}->{$start}->{$field};
	}
      }
    }
  } until($#next == -1);

  return ($sources, $vals);
}

sub get_field($$$) {
  my $self = shift;
  my $key = shift;
  my $field = shift;
  my @next = ( $key );
  do {
    my $curr = shift @next;
    if (defined $self->{victims}->{$curr}->{$field}) {
      return $self->{victims}->{$curr}->{$field};
    }
    if (defined $self->{victims}->{$curr}->{inherit}) {
      push @next, split(/,/, $self->{victims}->{$curr}->{inherit});
      chomp @next;
    }
  } until($#next == -1);
  return undef;
}


sub lookup($$$$){
  my $self = shift;
  my $start = shift;
  my $input = shift;
  my $output = shift;
  push @{$output}, $start;
  my $vals = $self->flatten($start);
  my $i = 0;
  while($i <= $#{$input}){
    my $key = $start;
    my $arg = $input->[$i++];
    my $answer;
    if(!defined $vals->{$arg}){
      $answer = '#';
    } else {
      $answer = $vals->{$arg};
    }
    push @{$output}, $answer;
  }
}

sub get_key($$) {
  my $self = shift;
  my $vic = shift;
  if (!defined $self->{victims}->{$vic}) {
    die "Victim '$vic' is not defined\n";
  }
  my %vals = $self->{victims}->{$vic};
  return \%vals;
}

sub print_all($$@) {
  my $self = shift;
  my $vic = shift;
  my $prefix = shift;
  if(!defined $self->{victims}->{$vic}){
    print STDERR "Victim '$vic' is not defined\n";
    return 2;
  }
  my $vals = $self->flatten($vic);
  foreach my $field (sort (keys %{$vals})){
    if (defined $prefix) {
      print "$prefix.$field $vals->{$field}\n";
    } else {
      print "$field $vals->{$field}\n";
    }
  }
  return 0;
}

sub match_key($$$) {
  my $self = shift;
  my $field = shift;
  my $val = shift;
  my @start = @_;
  my @keys;
  if ($#start == -1) {
    @start = keys %{$self->{victims}};
  }
  foreach my $vic (sort @start){
    my @input = @ARGV;
    next if(substr($vic,0,1) eq "_");
    my @output;
    my $tree = $self->flatten($vic);

    if (defined $tree->{$field} && $tree->{$field} eq $val) {
      push @keys, $vic;
    }
  }
  return @keys;
}


#
# $confs - ref to array containing conf files to read
# $func  - function to call for each line
# @func_args - args to function
#
sub read_conf_files($$@) {
  my $self = shift;
  my $func = shift;
  my @func_args = @_;

  my @confs = @{$self->{ConfFiles}};
  my %files;
  my $num_read = 0;
  while ($#confs!=-1) {
    my $fh;
    my $conf = shift @confs;
    my @words = split(/\s+/, $conf);
    if ($words[0] eq "connect") {
      $fh = new IO::Socket::INET(PeerAddr => $words[1]);
      if (!defined $fh) {
	die "Can't open: $conf: $? $!";
      }
      goto FOUND;
    }

    if (!($conf =~/^\// && -r $conf)) {
      foreach my $p (@{$self->{ConfPath}}) {
	if(-r "$p/$conf") {
	  $conf = "$p/$conf";
	  $fh = new IO::File $conf, "r";
	  if (!defined $fh) {
	    die "Can't open: $conf: $? $!";
	  }
	  $conf = abs_path($conf);
	  goto FOUND;
	}
      }
      next;
    }

    $fh = new IO::File $conf, "r";
    $conf = abs_path($conf);
  FOUND:
    next if($files{$conf}==1);
    $files{$conf} = 1;

    my $lineno = 0;

    while (<$fh>) {
      ++$lineno;
      my $line = $_;
      next if(/^#/);

      next if(/^\s*$/);
      my @words = split;
      chomp @words;

      if ($words[0] eq "include" && $#words == 1) {
	my $file = $words[1];
	debug "Check file: $file\n";
	unshift @confs, $file;
	next;
      }

      if ($words[0] eq "connect" && $#words == 1) {
	debug "Check connect: $words[1]\n";
	unshift @confs, $words[0] . " " . $words[1];
	next;
      }
      if (!/^\s*([\S^\.]+)\.([\S^=]+)\s*=\s*((\S.*)*)\s*$/) {
	die "Malformed line: $conf:$lineno\n";
      }

      $self->{sources}->{$1}->{$2} = "$conf:$lineno";
      $func->(@func_args, $1, $2, $3);
    }
#    $fh->close;
    ++$num_read;
  }
  if($num_read == 0){
    die "Can't read any configuration file!!!\n";
  }
}


sub store_conf_line($$$$) {
  my $self = shift;
  (my $l1, my $l2, my $l3) = @_;
  $self->{victims}->{$l1}->{$l2} = $l3;
#  print "Parse line $l1 $l2 $l3\n";
}



sub new () {
  my ($class, %arg) = @_;
  my $self = {};
  %{$self} = %arg;

  # Calculate default config files, and default paths
  if (!defined $self->{NoDefaults} || $self->{NoDefaults} == 0) {
    push @{$self->{ConfFiles}}, 
				"$Bin/../lib/victims.conf",
				"$Bin/../../lib/victims.conf",
				"$ENV{HOME}/.victims.conf";
  }

  push @{$self->{ConfPath}}, "$ENV{HOME}", "$Bin/../lib", "$Bin/../../lib";


  read_conf_files($self, *store_conf_line, $self);
  bless ($self, $class);
}


1;
