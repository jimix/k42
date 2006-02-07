# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: find_modified_files.pl,v 1.1 2005/09/01 21:29:32 neamtiu Exp $
# ############################################################################
#!/usr/bin/perl

# Find modified .[CH] files in a source tree. Can either use 'cvs diff', or simple 'diff'


####### pragmas ######
use strict;
use Getopt::Std;
######################

sub myDie {
    my $cause = shift;

    printf STDERR $cause."\n";

    exit 1;
}

sub doCvsDiff {
    
    my @res = `cvs diff|grep '.[CH],v'`;
    return @res;
}

sub doDiff {

    my $oldDir = shift;

    if  (! ( -d $oldDir )) {
        myDie "$oldDir is not a directory\n";
    }
    my @cres = `diff -r  $oldDir . |grep '.[CH]'`;
    my @res=();
    my $foo;

    foreach $foo (@cres) { 
        my $a; my $b; my $c; my $d; my $rest;

        ($a,$b,$c,$d,$rest) = split(/\s+/, $foo);

        push(@res, $d);
    }
    return @res;
}

sub doMakefile {

    myDie "use of Makefile targets not implemented yet";
}

sub usage {

    print STDERR "find_modified_files.pl WORK_DIR OPTION\n".
        "Options:\n".
        "-c              use cvs diff\n".
        "-2 old_dir      use diff beween old_dir and work_dir\n".
        "-m              use Makefile rebuild targets\n";

    exit 1;
}

#################### main ##################

$#ARGV >=1 or usage(); 

my $workDir = $ARGV[$#ARGV];

if  (! ( -d $workDir )) {
    myDie "WORK_DIR:$workDir is not a directory\n";
}

my %opts=();
getopts('cm2:',\%opts);

if ((($opts{c}) && ($opts{m})) ||
    (($opts{c}) && ($opts{2})) ||
    (($opts{2}) && ($opts{m}))) {

    myDie "Only one OPTION (-c, -2 or -m) can be specified";
}

chdir ($workDir) or myDie "can't cd into $workDir";


my @fileList = ();

if ($opts{c}) {

    @fileList = doCvsDiff();

} elsif ($opts{m}) {

    @fileList = doMakefile();

} elsif ($opts{2}) {
    
    @fileList = doDiff($opts{2});

} else {

    myDie "At least one OPTION (-c, -2 or -m) must be specified.";
}

print join("\n", @fileList) ;
