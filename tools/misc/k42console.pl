#!/usr/bin/env perl
# K42: (C) Copyright IBM Corp. 2004, 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

use strict;
use Getopt::Long;
Getopt::Long::Configure qw(no_ignore_case);
use Sys::Hostname;
use Socket;


my $verbose = 0;
my $imgver = 0;

my $defaults;
my $site;

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

##############################################################################
#
# Read from kvictim
#
#
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

##############################################################################
#
# Expand symlinks in a path
#
#
sub expand_path($)
{
  my $p = shift;
  my @path = split /\//, $p;
  my @expand;
  foreach my $x (@path) {
    if($x ne ""){
      $p = "";
      if($#expand != -1){
	$p = '/' . join('/', @expand);
      }

      my $l = readlink $p . '/' . $x;

      # Absolute link is resolved recursively
      if(substr($l,0,1) eq '/'){
	@expand = split '/' , expand_path($l);
      } else {
	# Relative link, append result to what we've got already
	if($l eq "") {
	  $l = $x;
	}
	push @expand, $l;
      }
    }
  }
  $p = '/' . join('/', @expand);
  $p =~ s/\/\//\//g;
  return $p;

}



#######################################################################
#
# Do DNS name resolution
#
sub hostNumeric($){
  my $host = shift;
  # get a non-loopback IP address for NFS host
  {
    (my $name, my $aliases, my $addrtype, my $length, my @addrs) =
      gethostbyname $host;

    if ($#addrs < 0){
      print "k42console: WARN: could not resolve IP address for: $host\n";
      return $host;
    }

    do {
      my $x = pop @addrs;
      $host = inet_ntoa($x);
      unshift @addrs;
    }while($host eq "127.0.0.1" && $#addrs>=0);
  }
  return $host;
}

########################################################################
#
# Parse output of kuservalues
#
sub readUserValues() {
  # Get default values by parsing kuservalues output
  open(VALS,"kuservalues|");
  while(<VALS>){
    my $line = $_;
    (my $var, my $val) = split /[\s=]+/, $line;
    if($val=~/\S/ && !defined $defaults->{$var}->{value}){
      $defaults->{$var}->{value} = $val;
      $defaults->{$var}->{source} = "kuservalues";
    }
  }
}

########################################################################
#
# Copy boot parameters into boot image
#
sub copyKParms($$)
{
  my($source) = shift;
  my($image) = shift;
  my($dummy);

  my($fileOffset) = `powerpc-linux-objdump -h $image | grep k42_boot_data`;
  if(($?/256) != 0) {
    ($fileOffset) = `objdump -h $image | grep k42_boot_data`;
  }
  (($?/256) == 0) || die "Could not find boot parameters section in $image\n";

  chomp($fileOffset);
  $fileOffset =~ s/^\s+//;
  ($dummy, $dummy, $dummy, $dummy, $dummy, $fileOffset, $dummy) = split(/\s+/, $fileOffset);
  print ("$fileOffset\n");
  $fileOffset = hex($fileOffset);

  print("Writing $source to $image at offset $fileOffset\n");
  print "dd bs=1 if=$source of=$image seek=$fileOffset conv=notrunc\n";
  if (system("dd bs=1 if=$source of=$image seek=$fileOffset conv=notrunc")/256!=0)
  {
    die "Could not write boot parameters from $source into $image\n";
  }
}


#############################################################################
#
# Die
#
sub abort($$){
  my $msg = shift;
  my $code = shift;
  print "ERROR: $msg";
  usage();
  exit $code;
}


sub usage() {
print <<EOF;
'k42console' installs K42 kernels on and remotely reboots victim machines

Usage: k42console OPTIONS [-- A=B ...]
 -A, --arch A          Assume architecture A
 -c, --config F1 ...   Read configuration file(s) F1, F2 ...
 -g, --get             Acquire lock on victim machine
 -i, --imgver N        Assume version N of K42 packages (sets K42_PKGVER)
 -k, --kanchor D       Assume directory D contains kitchsrc, install, etc.
 -o, --optimization L  Use optimization level L; one of noDeb, partDeb, fullDeb
 -n, --nfshost H       Mount user directories from host H (sets K42_NFS_HOST)
 -p, --pkghost H       Mount K42 packages from host H (sets K42_PKGHOST)
 -E, --experiment P    Treat P as a path, and run P/sysinit as init script
 -S, --no-new-parms    Do not write boot parameters to ELF section of image
 -N, --no-boot         Exit after creating and possibly writing boot parameters
     --nodisks         Do not use disks
 -h, --help            Show this message and exit
 -v, --verbose L       Be verbose; L is 1 (status messages), 2 (debug messages)
 -t, --timeout N       Kill thinwire and exit if N seconds have passed
 -f, --file F          Use executable image F as the boot image
 -D, --directory D     Act as if invoked from directory D (guess arch, debug)
 -l, --list            Dump all the environment variables and exit,
                       where output is in valid configuration file syntax
 -B, --breaklocks A    Break lock on victim; A is kill, steal, or killsteal,
                       where kill means stop ktw, steal means change ownership
 -M, --message S       Put message string S in the lock file
 -R, --reboot [A]      Power cycle victim; if A is only, stop afterwards
 -m, --machine M       Use victim M (if M is mambo, start simulator)

Examples:
 k42console -m mambo -v 2 -- K42_PKGROOT=partDeb.Img.3 K42_PKGHOST=1.2.3.4
 k42console -m hal -R -B killsteal

EOF
}



my $directory;
my $anchor;
my $imgver;
my $machine;
my $reboot;
my @config;
my $breaklocks;
my $timeout;
my $hwcfg;
my $printenv;
my $experiment;
my $username;
my $help;
my $release;
my $status;
my $acquire;
my $lockMsg;
my $bootfile;
my $nodisks;
my $pkghost;
my $nfshost;
my $optimization;
my $arch;
my $list;
my $suppress_new_boot_parms;
my $no_boot;

GetOptions(
	   "A|arch=s" => \$arch,
	   "B|breaklocks=s" => \$breaklocks,
	   "D|directory=s" => \$directory,
	   "M|message=s" => \$lockMsg,
	   "R|reboot:s" => \$reboot,
	   "c|config=s" => \@config,
	   "e" => \$printenv,
	   "f|file=s" => \$bootfile,
	   "g|get" => \$acquire,
	   "h|help"	=> \$help,
	   "i|imgver=i" => \$imgver,
	   "k|kanchor=s" => \$anchor,
	   "l|list"	=> \$list,
	   "m|machine=s" => \$machine,
	   "n|nfshost=s" => \$nfshost,
	   "nodisks" => \$nodisks,
	   "o|optimization=s" => \$optimization,
	   "p|pkghost=s" => \$pkghost,
	   "t|timeout=i" => \$timeout,
	   "v|verbose=i" => \$verbose,
           "E|experiment=s" => \$experiment,
           "S|no-new-parms" => \$suppress_new_boot_parms,
	   "N|no-boot" => \$no_boot
);

if(defined $help){
  usage;
  exit;
}



# Get pwd, don't trust environment
my $pwd = $ENV{PWD};
$pwd = `pwd`;
chomp $pwd;
if(! -d $pwd){
  undef $pwd;
}

if(defined $directory){
  if(! -d $directory || !chdir $directory){
    abort "Cannot chdir to $directory\n", 1;
  }
  $pwd = $directory;
}

$pwd = expand_path $pwd;


sub remove_entry($){
  my $name = shift;
  undef $defaults->{$name};
}

sub add_entry($){
  my $x = shift;
  $x->{source}= "k42console computed defaults";
  $defaults->{$x->{name}} = $x;
}


my $defenv = [ { 'name' => 'K42_ARCH',
		 'required' => 1,
		 'value' => "powerpc",
	       },

	       { 'name' => 'K42_SESSION_ID',
		 'setval' => sub ($) {
		   return $$;
		 },
	       },

	       { 'name' => 'K42_INITSCR',
		 'setval' => sub ($) {
		   my $x = shift;
		   if(defined $experiment){
		     return "$experiment/sysinit";
		   }
		   return "/kbin/sysinit";
		 },
	       },

	       { 'name' => 'K42_NFS_HOST',
		 'depends' => [ 'OPTIMIZATION', 'K42_ARCH', 'MKANCHOR' ],
		 'setval' => sub ($) {
		   my $p;
		   my $mkanchor = $ENV{MKANCHOR};
		   my $arch = $ENV{K42_ARCH};
		   my $opt = $ENV{OPTIMIZATION};
		   # Expand local symlinks
		   $p = expand_path "$mkanchor/install/$arch/$opt/kitchroot";

		   # Identify NFS mount point (if any)
		   my @x = split /\s+/, `df -P $p | tail -1`;

		   my $srv = $x[0];
		   if($srv =~ /(.*):(.*)/){
		     my $host = $1;
		     my $hostpath = $2;
		     my $mnt = $x[$#x];

		     $p = hostNumeric($host);
		   } else {
		     my $o = hostname();
		     $p = hostNumeric($o);
		     if ($p eq "127.0.0.1") {
			 print "ERROR: resolved $p as IP address of " .
                               "K42_NFS_HOST ($o).\n";
                         print "ERROR: please supply " .
                               "K42_NFS_HOST=<routable-ip> as argument.\n";
                         print "(Use hwconsole if you are trying to " .
                               "boot Linux or something other than K42.)\n";
			 exit 1;
		     }
		   }
		   return $p;
		 },

		 'checkval' => sub ($) {
		   my $x = shift;
		   if(defined $ENV{$x->{name}}){
		     $ENV{$x->{name}} = hostNumeric($ENV{$x->{name}});
		     #clear the check val field, never get called again
		     undef $x->{checkval};
		   }
		 },
	       },


	       { 'name' => 'K42_NFS_ROOT',
		 'depends' => [ 'K42_NFS_HOST', 'OPTIMIZATION',
				'K42_ARCH', 'MKANCHOR' ],
		 'setval' => sub ($) {
		   my $p;
		   my $host = $ENV{K42_NFS_HOST};
		   my $mkanchor = $ENV{MKANCHOR};
		   my $arch = $ENV{K42_ARCH};
		   my $opt = $ENV{OPTIMIZATION};
		   # Expand local symlinks
		   $p = expand_path "$mkanchor/install/$arch/$opt/kitchroot";

		   my $srv;
		   my $dir = $p;
		   while (1) {
		     # Identify NFS mount point (if any)
		     my @x = split /\s+/, `df -P $p | tail -1`;
		     chomp @x;
		     my $srv = $x[0];
		     my $mnt = $x[$#x];
		     if($srv =~ /(.*):(.*)/){
		       my $h = $1;
		       my $hostpath = $2;
		       $h = hostNumeric($h);
		       if($h ne $host){
			 print "WARNING: K42_NFS_HOST doesn't " .
			   "match mount point\n";
		       }
		       $srv = $host . ":" . $hostpath;

		       # Replace mount-point into the path
		       $p =~ s|$mnt|$srv|;
		       last;
		     } elsif(!-d $x[0]) {
		       $p = $host . ":" . $p;
		       last;
		     }
		     # Replace mount-point into the path
		     $p =~ s|$mnt|$srv|;

		   }
		   return $p;
		 },
		 'checkval' => sub ($) {
		   my $x = shift;
		   if(defined $ENV{$x->{name}}){
		     my($host, $path);
		     ($host, $path) = split(/:/, $ENV{$x->{name}}, 2);
		     $host = hostNumeric($host);
		     $ENV{$x->{name}} = "$host:$path";

		     #clear the check val field, never get called again
		     undef $x->{checkval};
		   }
		 }

	      },

	       { 'name' => 'OPTIMIZATION',
		 'required' => 1,
		 'setval' => sub ($) {
		   if($pwd=~/\/((no|part|full)Deb)\//){
		     return $1;
		   }else{
		     abort "Cannot determine optimization level in $pwd\n",1;
		   }
		   return undef;
		 },
		 'checkval' => sub ($) {
		   if(defined $ENV{OPTIMIZATION} && 
		      !($ENV{OPTIMIZATION}=~/^(fullDeb|partDeb|noDeb)$/)){
		     abort "Bad optimization level \"$ENV{OPTIMIZATION}\"\n",1;
		   }
		 },
	       },

	       # Identifty MKANCHOR
	       { 'name' => 'MKANCHOR',
		 'depends' => [ 'K42_ARCH' ],
		 'required' => 1,
		 'setval' => sub ($) {
		   my $arch = $ENV{K42_ARCH};
		   foreach my $x (("install", $arch, "kitchsrc")) {
		     if($pwd=~/(.*)$x/){
		       my $anchor = $1;
		       $anchor=~s|^(.*[^/])/*|$1|;
		       return $anchor;
		     }
		   }
		 },
	       },

	       { 'name' => 'K42_PKGROOT',
		 'depends' => [ 'OPTIMIZATION', 'K42_PACKAGES', 'K42_PKGVER' ],
		 'setval' => sub ($) {
		     if ($ENV{K42_PKGROOT} ne "") {
                       return $ENV{K42_PKGROOT};
		     } else {
                       my $path = $ENV{K42_PACKAGES};
                       return "$path/$ENV{OPTIMIZATION}.Img.$ENV{K42_PKGVER}";
		     }
		 },
	       },

	       { 'name' => 'MAMBO_KFS_PKGDISK',
		 'depends' => [ 'OPTIMIZATION', 'MAMBO_KFS_DISKPATH', 
				'K42_PKGVER' ],
		 'setval' => sub ($) {
		   my $path = $ENV{MAMBO_KFS_DISKPATH};
		   return "$path/KFS$ENV{OPTIMIZATION}.Img.$ENV{K42_PKGVER}";
		 },
	       },

	       { 'name' => 'K42_IP_HOSTNAME',
		 'depends' => [ 'HW_VICTIM' ],
		 'setval' => sub ($) { return $ENV{HW_VICTIM}; }
	       },

	       { 'name' => 'K42_PKGHOST',
		 'checkval' => sub ($) {
		   my $x = shift;
		   if(defined $ENV{$x->{name}}){
		     $ENV{$x->{name}} = hostNumeric($ENV{$x->{name}});
		     #clear the check val field, never get called again
		     $x->{value} = $ENV{$x->{name}};
					
		     undef $x->{checkval};
		   }
		 },
	       },

	       { 'name' => 'K42_FS_DISK',
		 'setval' => sub ($) {
		   if(!defined $nodisks){
		     return $hwcfg->{fs_disks};
		   }
		   return undef;
		 },
	       },

	       { 'name' => 'HW_BOOT_FILE',
		 'depends' => [ 'HW_VICTIM', 'MKANCHOR', 'K42_ARCH',
				'OPTIMIZATION' ],
		 'setval' => sub($) {
		   if($ENV{HW_VICTIM} eq "mambo"){
		     $ENV{HW_BOOT_FILE} = "$ENV{MKANCHOR}/$ENV{K42_ARCH}" .
					  "/$ENV{OPTIMIZATION}/os/" .
					  "mamboboot.tok";
		   } else {
		     $ENV{HW_BOOT_FILE} = "$ENV{MKANCHOR}/$ENV{K42_ARCH}" .
					  "/$ENV{OPTIMIZATION}/os/" .
					  "chrpboot.tok";
		   }
		 }
	       },


	       { 'name' => 'MAMBO_SIMULATOR_PORT',
		 'depends' => [ 'USR_BASE_PORT' ],
		 'setval' => sub ($) {
		   return $ENV{USR_BASE_PORT};
		 },
	       },

	       { 'name' => 'MAMBO_DEBUG_PORT',
		 'depends' => [ 'USR_BASE_PORT' ],
		 'setval' => sub ($) {
		   return $ENV{USR_BASE_PORT} + 10;
		 },
	       },

	       { 'name' => 'MAMBO_CONSOLE_PORT',
		 'depends' => [ 'USR_BASE_PORT' ],
		 'setval' => sub ($) {
		   return $ENV{USR_BASE_PORT} + 20;
		 },
	       },

	       { 'name' => 'MAMBO_DIR',
		 'depends' => [ 'MAMBO_TYPE' ],
		 'setval' => sub ($) {
		   my $x;
		   foreach $x (split /:/, $ENV{PATH}){
		     if(-x "$x/mambo-$ENV{MAMBO_TYPE}"){
		       return $x . "/..";
		     }
		   }
		 },
	       },

	       { 'name' => 'MAMBO_TYPE',
		 'setval' => sub ($) {
		   if($machine=~/mambo/){
		     return "$hwcfg->{mambo_type}";
		   }
		   return undef;
		 },
	       },
	       { 'name' => 'K42_TOOLSDIR',
		 'depends' => [ 'MKANCHOR' ],
		 'setval' => sub($) {
		   return "$ENV{MKANCHOR}/install/tools/";
		 },
	       },

	       { 'name' => 'MAMBO_OS_TCL',
		 'depends' => [ 'K42_TOOLSDIR' ],
		 'setval' => sub($) {
		   return "$ENV{K42_TOOLSDIR}/lib/k42.tcl";
		 },
	       },

	       { 'name' => 'MAMBO_TCL_STAMP',
		 'value' => '-05222005',
	       },

	       { 'name' => 'MAMBO_UTIL_TCL',
		 'depends' => [ 'K42_TOOLSDIR' ],
		 'setval' => sub($) {
		   return "$ENV{K42_TOOLSDIR}/lib/utils.tcl";
		 },
	       },

	       { 'name' => 'K42_LOGIN_PORT',
		 'depends' => [ 'USR_BASE_PORT' ],
		 'setval' => sub ($) {
		   if($machine=~/mambo/){
		     if(defined $ENV{USR_BASE_PORT}){
		       return $ENV{USR_BASE_PORT} + 13;
		     }
		     return undef;
		   }
		   return 513;
		 },
	       },

	       { 'name' => 'MAMBO_KFS_DISK_LOC',
		 'depends' => [ 'OPTIMIZATION', 'K42_ARCH', 'MKANCHOR' ],
		 'setval' => sub ($) {
		   if(defined $nodisks){
		     return undef;
		   }
		   return "$ENV{MKANCHOR}/$ENV{K42_ARCH}" . 
		     "/$ENV{OPTIMIZATION}/os";
		 },
	       },

	       { 'name' => 'MAMBO_PERCS_TCL',
		 'setval' => sub ($) {
		   my $file = "kernel/bilge/percs/percs.tcl";
		   if( -e $file){
		     return $file;
		   }
		   return undef;
		 },
		 'checkval' => sub ($) {
		   my $x = shift;
		   if(! -e $ENV{$x->{name}}){
		     undef  $ENV{$x->{name}};
		   }
		 },
	       },

	       { 'name' => 'TW_SIMULATOR_HOST',
		 'value'=> 'localhost',
	       },

	       { 'name' => 'K42_KPARMS_FILE',
		 'value'=> "kparm.data.$$",
	       },

	       { 'name' => 'K42_NFS_ID',
		 'setval' => sub ($) {
		   my $x = `id -u` . ":" . `id -g`;
		   $x =~s/\n//g;
		   return $x;
		 },
	       },

	       { 'name' => 'TW_COMMAND_LINE',
		 'depends' => [ 'TW_SIMULATOR_HOST', 'TW_BASE_PORT' ],
		 'setval' => sub ($) {
		   my $cmd = join(" ",
				  "thinwire2",
				  "$ENV{TW_SIMULATOR_HOST}:" . 
				  ($ENV{TW_BASE_PORT}-1),
				  $ENV{TW_BASE_PORT},
				  $ENV{TW_BASE_PORT} + 1,
				  $ENV{TW_BASE_PORT} + 2,
				  $ENV{TW_BASE_PORT} + 3,
				  $ENV{TW_BASE_PORT} + 4,
				 );
		   return $cmd;
		 },
	       },

	       { 'name' => 'TW_SIMIP_CMD',
		 'depends' => [ 'TW_SIMULATOR_HOST', 'TW_BASE_PORT' ],
		 'setval' => sub ($) {
		   my $cmd = join(" ",
				  "simip",
				  "$ENV{TW_SIMULATOR_HOST}:" .
				  ($ENV{TW_BASE_PORT} + 2) ,
				  "$ENV{TW_SIMULATOR_HOST}:" .
				  ($ENV{TW_BASE_PORT} + 4) ,
				 );
		   return $cmd;
		 },
	       },


	       # User specific value, USR_BASE_PORT from kuservlaues
	       # applied only if using sim
	       { 'name' => 'TW_BASE_PORT',
		 'depends' => [ 'USR_BASE_PORT' ],
		 'setval' => sub ($) {
		   if($machine=~/mambo/){
		     return $ENV{USR_BASE_PORT}+1;
		   }
		   return undef;
		 },
	       }
	     ];

for(my $x = 0; $x <= $#{$defenv}; ++$x){
  add_entry($defenv->[$x]);
}


my $kfs_disk_root = { 'name' => 'MAMBO_KFS_DISK_ROOT',
		      'depends' => [ 'MAMBO_KFS_DISK_LOC' ],
		      'setval' => sub ($) {
			my $x = shift;
			my $file = "$ENV{MAMBO_KFS_DISK_LOC}/$x->{data}";
			if(-e $file ){
			  return $file;
			}
			return undef;
		      },
		      'checkval' => sub ($) {
			my $x = shift;
			if(! -e $ENV{$x->{name}}){
			  undef $ENV{$x->{name}};
			}
		      },
		      'data' => "DISK0.0.1",
		    };

my $kfs_disk_kitchroot = { 'name' => 'MAMBO_KFS_DISK_KITCHROOT',
			   'depends' => [ 'MAMBO_KFS_DISK_LOC' ],
			   'setval' => $kfs_disk_root->{setval},
			   'checkval' => $kfs_disk_root->{checkval},
			   'data' => "DISK0.0.0",
			 };

my $kfs_disk_extra = { 'name' => 'MAMBO_KFS_DISK_EXTRA',
		       'depends' => [ 'MAMBO_KFS_DISK_LOC' ],
		       'setval' => $kfs_disk_root->{setval},
		       'checkval' => $kfs_disk_root->{checkval},
		       'data' => "DISK0.0.2",
		     };

add_entry($kfs_disk_root);
add_entry($kfs_disk_kitchroot);
add_entry($kfs_disk_extra);

if($verbose>0){
  $ENV{HW_VERBOSE}=$verbose;
} elsif(defined $ENV{HW_VERBOSE}){
  $verbose = $ENV{HW_VERBOSE};
}

# Get the name of our site
my @v = split /\s+/, `kvictim site name`;
chomp @v;
$site = $v[1];

readUserValues;


# Read config files
while($#config != -1){
  my $file = shift @config;
  die "Unreadable config file $file\n" if(! -r $file);

  open CONFIG, "<$file";
  while(<CONFIG>){
    my $line = $_;
    next if($line=~/^#/);
    next if($line=~/^\s*$/);

    if($line=~/^\s*([^\s=]+)\s*=\s*(.*)$/){
      $defaults->{$1}->{value} = $2;
      $defaults->{$1}->{source} = "config file";
      $ENV{$1}=$2;
    }
  }
}


# Anything following "--" is parsed as "NAME=VALUE" and inserted into env
while($#ARGV!=-1){
  my $arg = $ARGV[0];
  if($arg=~/^([^=]+)=(.*)$/){
    $defaults->{$1}->{value}=$2;
    $defaults->{$1}->{source}='cmdline';
    $ENV{$1} = $2;
  }
  shift @ARGV;
}

# Associate command line args with env var names
my @bindings = ( \$imgver,	'K42_PKGVER',
		 \$nfshost,	'K42_NFS_HOST',
		 \$arch,	'K42_ARCH',
		 \$optimization,'OPTIMIZATION',
		 \$anchor,	'MKANCHOR',
		 \$machine,	'HW_VICTIM',
		 \$bootfile,	'HW_BOOT_FILE',
		 \$pkghost,	'K42_PKGHOST');


# If cmd line arg not set, get definition from env var and vice versa
while($#bindings!=-1){
  my $var = shift @bindings;
  my $name= shift @bindings;
  if(!defined $$var && defined $ENV{$name}){
    $$var = $ENV{$name};
  } elsif(defined $$var){
    $ENV{$name} = $$var;
    $defaults->{$name}->{value}= $$var;
    $defaults->{$name}->{source}= 'cmdline';
  }
}

if(!defined $machine){
  print "k42console: please supply victim machine with -m, or see --help\n";
  exit 1;
}

if($machine=~/mambo/s){
  $hwcfg = parseConfig($site . "_" . $machine);
} else {
  $hwcfg = parseConfig($machine);
}

if(!defined $hwcfg){
  die "$machine is not a valid victim\n";
}


#
# Identify and "K42_*" values as default env values
#

for my $x (keys %{$hwcfg}){
  if($x=~/^K42_/ || $x=~/^MAMBO_/ || $x=~/^TW_/ || $x=~/^USR_/ || $x=~/^HW_/) {
    if(!defined $defaults->{$x}->{value}){
      $defaults->{$x}->{value} = $hwcfg->{$x};
      $defaults->{$x}->{source} = "kvictim";
    }
  }
}



#
# Scan all values setters, as long as ENV variables are being set, keep
# looping, since some variables are waiting for others to be set.
#
my $changes = 1;
while($changes){
  $changes = 0;
  for my $x (keys %{$defaults}) {
    if(! defined $ENV{$x}){
      my $val;
      if(defined $defaults->{$x}->{value}){
	$val = $defaults->{$x}->{value};
	
      } elsif(defined $defaults->{$x}->{setval}){
	# Check for dependencies
	my $ready = 1;
	if(defined $defaults->{$x}->{depends}){
	  my $depends = $defaults->{$x}->{depends};
	  foreach my $y (@{$depends}) {
	    if(!defined $ENV{$y}) {
	      $ready = 0;
	      last;
	    }
	  }
	}
	if($ready){
	  $val = $defaults->{$x}->{setval}($defaults->{$x});
	}
      }


      if(defined $val){
	$ENV{$x} = $val;
	$changes = 1;
      }
    } elsif(! defined $defaults->{$x}->{source}){
      $defaults->{$x}->{source} = "preset ";
    }

    # Run sanity checks on variables
    if(defined $defaults->{$x}->{checkval}){
      $defaults->{$x}->{checkval}($defaults->{$x});
    }
  }
}


if(defined $list || $verbose>1){
  open  DESC, "<$ENV{K42_TOOLSDIR}/lib/k42_env_desc.txt";
  while(<DESC>){
    my $line = $_;
    if($line =~/^(\S+)\s+(\S.*)$/){
      if(defined $defaults->{$1}){
	$defaults->{$1}->{desc} = $2;
      }
    }
  }
  close DESC;

  foreach my $x (sort(keys %{$defaults})){
    if(defined $ENV{$x}){
      print "# $x : $defaults->{$x}->{desc}\n";
      print "# from $defaults->{$x}->{source}:\n$x = $ENV{$x}\n\n";
    }
  }
}

foreach my $x (keys %ENV){
  if(!defined $ENV{$x}){
    delete $ENV{$x};
  }
}

if(defined $list) {
  exit 0;
}

# Generate kparm.data file
if (system("build_kern_parms -o kparm.data.$$")/256 != 0)
{
  print "Failed to build kernel parameter file\nExiting\n";
  exit(1);
}

# dd it into the kernel image
if ($suppress_new_boot_parms)
{
  print "Not copying environment into boot parameters part of boot image\n";
} else {
  copyKParms("kparm.data.$$", $ENV{HW_BOOT_FILE});
}

# In case all we have to do is build the new image with new paramters.
if ($no_boot) {
  exit(0);
}

my @args;
push @args, "hwconsole";

push @args, "-f";
push @args, $ENV{HW_BOOT_FILE};

if(defined $breaklocks) {
  push @args, "-B";
  push @args, $breaklocks;
}

if(defined $verbose){
  push @args, "-v";
  push @args, $verbose;
}

if(defined $timeout){
  push @args, "-t";
  push @args, $timeout;
}

if(defined $reboot){
  push @args, "-R";
}

exec @args
