#!/usr/bin/perl
#

# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: find-fref-diff.perl,v 1.1 2002/03/05 23:25:07 dilma Exp $
# ############################################################################
#
# argument: name of 2 text trace files (output of traceTool)

use strict;

#returns number of fref in file
sub process_file($$) {
    my $file = shift(@_);
    my $href = shift(@_);
    my $nb_frefs = 0;
    open(FILE, $file);
    my $line;
    while (<FILE>) {
	$line = $_;
	my @words = split ' ', $line;
	my ($ref, $name);
	if ( $line =~ m/TRACE_FS_FULL_FILE_NAME/) {
	    $ref = $words[3];
	    $name = $words[5];
	    if (!exists($$href{$ref})) {
		$$href{$name} = $ref;
		$nb_frefs++;
	    }
	}

#	if ($nb_frefs > 10) {
#	    last;
#	}
    }

    return $nb_frefs;
}

# given two sets A and B(each represented by a hash), returns the 
# list of elements  A - B in argument C
sub compute_set_diff($$$)
{
    my $aref = shift(@_);
    my $bref = shift(@_);
    my $cref = shift(@_);

    my $key;
    foreach $key (keys %$aref) {
	 if (!exists($$bref{$key})) {
	     push @$cref, $key;
	 }
     }
}


if ($#ARGV != 1) {
    print "$0: <trace1> <trace2>\n";
    die;
}

my $file1 = $ARGV[0];
my $file2 = $ARGV[1];

my %frefs1;
my %frefs2;

my $nb_frefs_1 = process_file($file1, \%frefs1);
print "trace $file1 contains $nb_frefs_1 refs to ServerFile objects\n";
my $nb_frefs_2 = process_file($file2, \%frefs2);
print "trace $file2 contains $nb_frefs_2 refs to ServerFile objects\n";

#my $key;
#print "elements in first hash:\n";
#foreach $key (keys %frefs1) {
#    print "\t$key element $frefs1{$key}\n";
#}
#print "DONE\n";

my @list_1_2;
compute_set_diff(\%frefs1, \%frefs2, \@list_1_2);
my @list_2_1;
compute_set_diff(\%frefs2, \%frefs1, \@list_2_1);

my $i;
if ($#list_1_2 != -1) {
    print "frefs in $file1 but not in $file2:\n";
    for ($i=0; $i<= $#list_1_2; $i++) {
	print "\t$list_1_2[$i]\n";
    }
}

if ($#list_2_1 != -1) {
    print "frefs in $file2 but not in $file1:\n";
    for ($i=0; $i<= $#list_2_1; $i++) {
	print "\t$list_2_1[$i]\n";
    }
}

