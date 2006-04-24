#!/usr/bin/env perl
#############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kconf.pl,v 1.3 2006/02/01 06:27:20 yeohc Exp $
# ############################################################################

use POSIX;
use Getopt::Long;

use FindBin qw($Bin);
use lib "$Bin/../lib";
use lib "$Bin/../../lib";
use KConf;
use Pod::Usage;

sub usage(){
print << "USAGE_EOF"
USAGE_EOF
}


# victims.conf in the lib directory (../../lib, ../lib) relative to kvictim is
# the default config file
my @sysconfs;




if($#ARGV==-1) {
  print STDERR "No victim specified to look up\n";
  exit -1;
}


Getopt::Long::Configure("bundling");
my $no_defaults = 0;
my $do_dump = 0;
my $do_list = 0;
my $show_help = 0;
my $find_match;
my @targets;
my @conf_files;
my @get_fields;
GetOptions('conffile|c=s@' => \@conf_files, # Specify config files to read
	   'nodefault|n+'   => \$no_defaults, # Don't read default configs
	   'origin|o'	=> \$do_origin,
	   'list|l+'	=> \$do_list,
	   'find|f=s'	=> \$find_match,
	   'get|g=s@'	=> \@get_fields,
	   'help|h+'	=> \$show_help,
	   'show|s=s@'	=> \@targets,		# Victims to show
) or pod2usage(2);

pod2usage(-verbose =>2) if $show_help;

my $key;
my $val;
format Listing =
@<<<<<<<<<<<<<<<<<<<<<< <- @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$key,			   $val
.
$~ = 'Listing';

my $kconf = KConf->new(NoDefaults => $no_defaults, ConfFiles => \@conf_files);

if (defined $find_match) {
  if (!($find_match =~/([\S^=]+)=(\S*)/)) {
    die "Wrong match syntax: $find_match\n";
  }
  my $field = $1;
  my $val = $2;
  my @keys = $kconf->match_key($field, $val);
  foreach my $x (@keys) {
    print "$x\n";
  }
  exit 0;
}

if ($#targets == -1) {
  @targets = keys %{$kconf->{victims}};
}


if ($do_list) {
  foreach my $vic (@targets) {
    my $x = $kconf->{victims}->{$vic};
    foreach my $k (keys %{$x}) {
      print "$vic.$k=$x->{$k}\n";
    }
  }
  exit 0;
}


if ($do_origin) {

  foreach my $vic (@targets) {
    (my $sources, my $values) = $kconf->flatten($vic);
    foreach my $k (keys %{$values}) {
      printf("%-32s <- %s\n", "$vic.$k", $sources->{$k});
    }
  }
  exit 0;
}


if ($#get_fields > -1) {
  foreach my $g (@get_fields) {
    if ($g =~ /([^\.]*)\.([^=]*)/) {
      my $key = $1;
      my $field = $2;
      my $x = $kconf->get_field($key, $field);
      if(!defined $x) {
	$x = "#";
      }
      print "$x\n";
    }
  }
  exit 0;
}


my $victims = $kconf->{victims};


if ($#targets >= 1) {
  foreach my $x (@targets) {
    $kconf->print_all($x, $x);
  }
} elsif($#targets == 0) {
  $kconf->print_all($targets[0]);
}

exit 0;
__END__

=head1 kconf

kconf - Examine kconf configurations.

=head1 SYNOPSIS

Examines/prints contents of KConf configurations.

  kconf [-s|--show <name>] [-o|--origin] [-l|--list] 
	[-n|--nodefault] [-c|--conffile <file>]

=head1 OPTIONS

=over 8

=item B<-s|--show <name>>

Select keys to display. May be specified more than once.  If only one
B<-s> option is given, (and no B<-o> or B<-l> options), then the
output will be in the format: "field value".  Otherwise the output
format will be "key.field value".

=item B<-o|--origin>

Show the origin of a key/field's value in a configuration space, 
rather than the value.

=item B<-c|--conffile>

Read the specified file as a configuration data file.

=item B<-n|--nodefaults>

Do not read default config files.

=item B<-l|--list>

List all key.field=value definitions.  If -s options are specified the
listing is restricted to the specified keys.  This option prints the
configuration files that have been read in as if the configuration has
been given in a single file (hides the effect of "include" and
"connect" directives).


=head1 KCONF CONFIGURATION

=head1 Keys and Values

Each key can have any arbitrary (field,value) pair, as necessary to
support the tools and environment for that key.  Here are some of the
fields used by the ktwd/thinwire/hwconsole tools.  Where indicated,
some fields are used only by site-specific tools, and may not be
applicable for other keys/sites. The syntax for specifying these is:

key.field = value

A key may specify an "inherit" field, which identifies a parent set of
field values that are searched if the field being sought does not
exist in the victims set of field values. Multiple inherit field
values are seperated by commas. For an inheritance tree such as this:


 A.inherit = B,C
 B.inherit = D,E
 C.inherit = F,G

            A
           / \
          B   C
         / \ / \
         D E F G

The search order is: A B C D E F G.

Once a field value is found, any subsequent definitions are ignored.

This inheritance mechanism allows one to define a set of common values
that can be easily associated with sets of keys to avoid replicated
key.value settings.


=head1 Including Configuration Files

A configuration can "include" in place another configuration
file. This is specified with an "include" directive:

include /path/to/included.conf

The effect of this is the same as if the contents of the specified
file were inserted at this location of the configuration file.

If the file to be included is not an absolute path, the program will
attempt to locate the file relative to ${LIBDIR} (as defined during
build/installation time).

=head1 Configuration Servers

As an alternative to "include" directives, one may specify a
hostname/TCP-port pair that kconf will connect to to retrieve
configuration data:

connect localhost:9999

To support this mechanism, one may use kconf in conjunction with inetd
to export a configuration file, with an inet.conf line such as:

9999 stream tcp nowait  root /home/mostrows/bin/kconf 
kconf -l -n -c /path/to/exported.conf

Note the use of the B<-l> option to export the configuration that is
read in.  Also, the -n option is used to inhibit reading any implicit
configuration files.

=head1 Default Configuration Files

Unless inhibited by B<-n>, the following files are read in:

 ${HOME}/.victims.conf
 ${LIBDIR}/victims.conf

LIBDIR is the installation location defined at build/installation time.

=cut
