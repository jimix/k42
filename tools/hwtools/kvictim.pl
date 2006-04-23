#!/usr/bin/perl
#############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kvictim.pl,v 1.5 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################

use POSIX;
use Getopt::Long;
sub usage(){
print << "USAGE_EOF"
Usage:

  kvictim name field1 field2 ....
  kvictim field=X field1 field2 field3 ....
  kvictim all victim_name

Invocation:
  kvictim name field1 field2 ....

Returns:

  name	value1	value2 ....

"name" is the name of a victim (e.g. 'k0') "field*" are names of
fields in the tables defined below.  Thus this invocation returns a
string with the name of the victim and the values for all specified
fields.

Invocation:

  kvictim field=X field1 field2 field3 ....

Returns:
  nameA	value1	value2	value3 ....	
  nameB	value1	value2	value3 ....	
  nameC	value1	value2	value3 ....	

Returns a list of all victims for which the value of "field" is "X",
along with the values of all subsequently specified fields.

The definitions of fields and values are all contained within the
kvictim script.

The value for any field that is not defined for a particular victim is "#".

Invocation:
  kvictim all name

Returns:
  field1 value1
  field2 value2
  field3 value3
  ...

Returns a list of all fields and values for a specified name.


Read the 'victims.conf' configuration file for more details.

USAGE_EOF
}

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

#
# Return a flat hash of all values for a given victim name
#
sub flatten($){
  my $start = shift;
  #
  # Print out  field, value pairs,  for each field defined
  # for the specified victim.
  #
  my @next;
  push @next, $start;

  my $vals;
  do {
    $start = shift @next;
    foreach my $field (keys %{$victims->{$start}}) {
      if($field eq "inherit"){
	# Remember inheritance keys to look up later
	push @next, split(/,/, $victims->{$start}->{$field});
      } else {
	if(!defined $vals->{$field}){
	  $vals->{$field} = $victims->{$start}->{$field};
	}
      }
    }
  } until($#next == -1);

  return $vals;
}


sub lookup($$$){
  my $start = shift;
  my $input = shift;
  my $output = shift;
  push @{$output}, $start;
  my $vals = flatten($start);
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

sub print_all($@) {
  my $vic = shift;
  my $prefix = shift;
  if(!defined $victims->{$vic}){
    print STDERR "Victim '$vic' is not defined\n";
    return 2;
  }
  my $vals = flatten($vic);
  foreach my $field (sort (keys %{$vals})){
    if (defined $prefix) {
      print "$prefix.$field $vals->{$field}\n";
    } else {
      print "$field $vals->{$field}\n";
    }
  }
  return 0
}

sub print_match($$) {
  my $field = shift;
  my $val = shift;
  foreach my $vic (sort (keys %{$victims})){
    my @input = @ARGV;
    push @input, $field;
    next if(substr($vic,0,1) eq "_");
    my @output;
    lookup($vic, \@input, \@output);
    if($output[$#output] eq $val) {
      pop @output;
      print join("\t", @output) . "\n";
    }
  }
  return 0;
}




# victims.conf in the lib directory (../../lib, ../lib) relative to kvictim is
# the default config file
my @sysconfs;

my $do_debug = 0;
sub debug($) {
  my $x = shift;
  if ($do_debug == 0) { return; }
  print "# $x";
}

my $oldstyle = 0;
my $exec = $0;
my @defconf;
my @conf_files;
my $basepath = $exec;

if ($0 =~/kvictim/) {
  $oldstyle = 1;
}

$basepath =~s/^(.*)$exec/$1..\/lib\//;
my $sysconf = $basepath . "victims.conf";

if(! -r $sysconf) {
  $basepath = $exec;
  $basepath =~s/^(.*)kvictim/$1..\/..\/lib\//;
  $sysconf = $basepath . "victims.conf";
}

my @homeconfs = ( "$ENV{HOME}/.victims.conf",
		  "/home/" . getpwuid(getuid) . "/.victims.conf" );

my $homeconf;
while($#homeconfs >= 0) {
  $homeconf = shift @homeconfs;
  last if(-r $homeconf );
  undef $homeconf;
}


if(defined $homeconf){
  debug "Add file $homeconf\n";
  unshift @defconf, $homeconf;
}
debug "Add file $sysconf\n";
unshift @defconf, $sysconf;

$victims->{kvictim}->{confpath} = $basepath;



#
# $confs - ref to array containing conf files to read
# $func  - function to call for each line
# @func_args - args to function
#
sub read_conf_files($$@) {
  my $confs = shift;
  my $func = shift;
  my @func_args = @_;
  my %files;
  my $num_read = 0;
  while ($#{@{$confs}}!=-1) {
    $conf = shift @{$confs};

    next if(!-r "$conf");
    next if($files{$conf}==1);

    $files{$conf} = 1;

    my $lineno = 0;
    open DATA, "<$conf";
    while (<DATA>) {
      ++$lineno;
      my $line = $_;
      next if(/^#/);

      next if(/^\s*$/);
      my @words = split;
      chomp @words;
      if ($words[0] eq "include") {
	$file = $words[1];
	if (-r $file) {
	  debug "Add include file $file\n";
	  unshift @{$confs}, $file;
	} elsif (-r "$basepath/$file") {
	  debug "Add include file $basepath/$file\n";
	  unshift @{$confs} , "$basepath/$file";
	}
	next;
      }
      if (!/^\s*([\S^\.]+)\.([\S^=]+)\s*=\s*(\S.*)\s*$/) {
	die "Malformed line: $conf:$lineno\n";
      }
      $func->(@func_args, $1, $2, $3);
    }
    close DATA;
    ++$num_read;
  }
  if($num_read == 0){
    die "Can't read any configuration file!!!\n";
  }
}

sub store_conf_line($$$$) {
  my $victims = shift;
  (my $l1, my $l2, my $l3) = @_;
  $victims->{$l1}->{$l2} = $l3;
#  print "Parse line $l1 $l2 $l3\n";
}

if($ARGV[0] eq "--usage" ||
   $ARGV[0] eq "--help"){
  usage();
  exit 0;
}
if($#ARGV==-1) {
  print STDERR "No victim specified to look up\n";
  exit -1;
}

if ($oldstyle == 1) {
  my $start = shift @ARGV; 

  push @conf_files, @defconf;

  read_conf_files(\@conf_files, *store_conf_line, $victims);

  if($start eq 'all') {
    #
    # Print out  field, value pairs,  for each field defined
    # for the specified victim.
    #
    exit print_all(shift @ARGV);
  }

  if($start=~/([A-Za-z0-9_]*)=([A-Za-z0-9_]*)/){
    my $field=$1;
    my $val=$2;
    exit print_match($field, $val);
  }
  my @output;

  if(!defined $victims->{$start}){
    print STDERR "Victim '$start' is not defined\n";
    exit 1;
  }


  lookup($start, \@ARGV, \@output);
  print join("\t", @output) . "\n";

  exit 0;
}


Getopt::Long::Configure("bundling");
my $no_defaults = 0;
my $do_dump;
my @targets;
GetOptions('conffile|c=s@' => \@conf_files, # Specify config files to read
	   'nodefault|n+'   => \$no_defaults, # Don't read default configs
	   'dump|d+'	=> \$do_dump,		# Dump processed config
	   'show|s=s@'	=> \@targets,		# Victims to show
);

if ($no_defaults == 0) {
  push @conf_files, @defconf;
}

print join(" ", @conf_files) . " $nodefaults\n";
print join(" ", @defconf) . " $nodefaults\n";

if ($do_dump) {
  sub dump_conf_line($$$) {
    (my $l1, my $l2, my $l3) = @_;
    $victims->{$l1}->{$l2} = $l3;
    print "$l1.$l2=$l3\n";
  }
  read_conf_files(\@conf_files, *dump_conf_line);
  exit 0;
}


read_conf_files(\@conf_files, *store_conf_line, $victims);

if ($#targets >= 1) {
  map { print_all $_, $_ } @targets;
} elsif($#targets == 0) {
  print_all $targets[0];
}
