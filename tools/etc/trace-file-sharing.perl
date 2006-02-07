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
#  $Id: trace-file-sharing.perl,v 1.3 2003/06/05 16:05:28 dilma Exp $
# ############################################################################

# argument: text trace file (output of traceTool)
#
# Purpose: process trace file events in order to determine if there was
#          concurrent access to a file.
#
# Assumptions: This relies on the following trace events.
# ( "element[0]" is the first element in the trace line)
# - TRACE_FS_ServerFile_CREATED: We use element[5] (ref) and
#                                element[9] (time)
# - TRACE_FS_STUB_ACQUIRE: element[3] (ref),element[5] (pid), element[7] (time)
# - TRACE_FS_STUB_ACCESS: element[3] (ref), element[5] (pid), element[7]
#                         (time), element[9] (operation)
# - TRACE_FS_STUB_RELEASE: element[3] (ref),element[5] (pid), element[7] (time)
# - TRACE_FS_NAME_TREE_ACCESS: element[3] (ref), element[5] (time) and
#                              element[7] (number of clients expect FR)
# - TRACE_FS_getObj_RET: element[3] (ref), element[5] (time) and
#                              element[7] (useType value)
# - TRACE_FS_DESTROY_INVOKED_BUT_NOT_DONE: element[3] (ref), element[5](time) FIXME this event is not generated anymore

use strict;

my %frefs;
my $nb_frefs = 0;

my @accesses; #each element is a list of accesses

# process_input reads data from input file and fills up global
# variables %frefs, $nb_frefs, @accesses
#
sub process_input() {
    my $line;
    
    while (<>) {
	$line = $_;
	my @words = split ' ', $line;
	my $ref;
	my $acc;
	my $pos;
	if ( $line =~ m/TRACE_FS_ServerFile_CREATED/) {
	    $ref = $words[5];
	    $acc = "$words[9] CREATED";
	} elsif ( $line =~ m/TRACE_FS_NAME_TREE_ACCESS/) {
	    $ref = $words[3];
	    $acc = "$words[5] NTA $words[7]";
	} elsif ( $line =~ m/TRACE_FS_getObj_RET/) {
	    $ref = $words[3];
	    $acc = "$words[5] getObj  $words[7]";
	} elsif ( $line =~ m/TRACE_FS_STUB_ACQUIRE/) {
	    $ref = $words[3];
	    $acc = "$words[7] STUBACQUIRE $words[5]";
	} elsif ( $line =~ m/TRACE_FS_STUB_ACCESS/) {
	    $ref = $words[3];
	    $acc = "$words[7] STUBACC $words[5] $words[9]";
	} elsif ( $line =~ m/TRACE_FS_STUB_RELEASE/) {
	    $ref = $words[3];
	    $acc = "$words[7] STUBRELEASE $words[5]";
	} elsif ( $line =~ m/TRACE_FS_DESTROY_INVOKED_BUT_NOT_DONE/) {
	    # FIXME this event is not generated anymore
	    $ref = $words[3];
	    $acc = "$words[7] DESTROY $words[5]";
	} else {
	    next;
	}
	
	if (!exists($frefs{$ref})) {
	    $frefs{$ref} = $nb_frefs;
	    $pos = $nb_frefs;
	    $nb_frefs++;
	} else {
	    $pos = $frefs{$ref};
	}
	push @{$accesses[$pos]}, $acc;
    }
}

sub print_accesses($) {
    my $ref = shift(@_);
    my $i = $frefs{$ref};
    print "access for $ref i is $i:\n";
    my @sorted = sort @{$accesses[$i]};
    my $j = 0;
    while ($j <= $#sorted) {
	print "\t$sorted[$j]\n";
	$j++;
    }
}

# Values returned by this routine:
# 0 if everything ok
# 1 if there is a NTA access with number clients > 0
# 2 if there is a STUB release without corresponidng STUB acquire
# 3 if there is a STUB ACCESS while other processes were in position of
#    accessing too 
# 4 if there is a STUB ACCESS without prior STUB acquire
sub check_accesses($) {
    my $ref = shift(@_);
    my $i = $frefs{$ref};
    my @sorted = sort @{$accesses[$i]};

    my @pid_seq;
    my $j = 0;
    while ($j < $#sorted) {
	my $acc = $sorted[$j];
	my $pid;
	my @parts = split " ", $acc;
	if ($acc =~ m/CREATED/) {
	    # skip it
	} elsif ($acc =~ m/NTA/) {
	    my $nc = $parts[2];
	    if ($nc != 0) {
		#print "NTA with nc $nc\n";
		return 1;
	    }
	} elsif ($acc =~ m/STUBACQUIRE/) {
	    $pid = $parts[2];
	    push @pid_seq, $pid;
	} elsif ($acc =~ m/STUBACC/) {
	    #if there is any other pid in pid_seq, indicate error
	    $pid = $parts[2];
	    my $size = $#pid_seq+1;
	    if ($size > 1) {
		#print "STUBACC by $pid when pid_seq has other pids(sze $size)\n";
		return 3;
	    } elsif ($size == 0) {
		#print "STUBACC by pid $pid without previous STUBACQUIRE\n";
		return 4;
	    } elsif ($pid_seq[$#pid_seq] != $pid) {
		#print "STUBACC by pid $pid without previous STUBACQUIRE\n";
		return 4;
	    }
	} elsif ($acc =~ m/STUBRELEASE/) {
	    #if pid does not appear in pid_seq, error
	    $pid = $parts[2];
	    my $k;
	    my $found = 0;
	    for ($k = 0; $k <= $#pid_seq; $k++) {
		if ($pid eq $pid_seq[$k]) {
		    # remove pid from pid_seq
		    splice @pid_seq, $k, 1;
		    $found = 1;
		    last;
		}
	    }
	    if ($found == 0) {
		# did not found element
		#print "STUBRELEASE without prior STUBACQ (pid $pid)\n";
		return 2;
	    }
	}
	$j++;
    }

    return 0;
}

process_input();
print "Number of ServerFile refs is $nb_frefs \n";
my $ref;

my @error_types = ("No error", 
		   "NTA access with nb clients > 0",
		   "STUB release without corresponding STUB acquire",
		   "STUB ACCESS: shared",
		   "STUB ACCESS without prior STUB acquire",
		   );
my @error_counter = (0, 0, 0, 0, 0,);

foreach $ref (keys %frefs) {
    my $ret = check_accesses($ref);
    if ($ret < 0 || $ret > 4) {
	printf "check_accesses returned invalid value ($ret) for ref $ref\n";
    } else {
	$error_counter[$ret]++;
    }
}

my $i;
for ($i=0; $i<=4; $i++) {
    print "$error_types[$i]: $error_counter[$i]\n";
}

