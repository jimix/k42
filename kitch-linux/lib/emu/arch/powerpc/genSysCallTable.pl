# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: genSysCallTable.pl,v 1.13 2005/08/31 13:56:05 dgurley Exp $

use warnings;
use strict;
my @table;
my %done;
my $outfile='-';
$outfile=$ARGV[1] if @ARGV > 1;
open(OUTFILE, ">$outfile") || die "Failed to open $outfile: $!\n";

sub readTable($) {
    my $infile = shift;
    my $i;
    open(INFILE, "<$infile") || die "Failed to open $infile: $!\n";

    $i = 0;
    while(<INFILE>) {
	/^#/ and next;
	/^\s*$/ and next;
	s /\s*\|\s*//;
	my @row = split / *\| */;
	@row >= 7 or die "Not enough fields in table row\n".$_."\n";
	$table[$i]->{"index"} = $row[0];
	$row[0] == $i || die "Index error line $i $row[1]\n";
	$table[$i]->{"linux"} = $row[1];
	$table[$i]->{"64"} = $row[2];
	$table[$i]->{"32"} = $row[3];
	$table[$i]->{"signextend"} = $row[4];
	$table[$i]->{"comment"} = $row[5];
	$table[$i]->{"format"} = $row[6];
	$i+=1;
    }
}

sub condPrint($) {
    my $key=$_[0];
    unless ($done{$key}) {
	$done{$key} = 1;
	print OUTFILE $key;
    }
}

readTable($ARGV[0]);

#testing stuff - remove once all is well
#generates a stand alone file which will compile
#  print OUTFILE "#include <stdarg.h>\n".
#      "#define passertMsg(x, ...)\n".
#      "typedef long int uval; typedef int sval; void verr_printf(const char* fmt, ...);\n" .
#      "typedef int sval32; void err_printf(const char* fmt, ...);\n" .
#      "struct kaka { int processID;};\n" .
#      "struct { struct kaka * dispatcher; } extRegsLocal;\n";

print OUTFILE "/*\n";
print OUTFILE " ***********************************************************\n";
print OUTFILE " *                                                         *\n";
print OUTFILE " *  GENERATED TABLE - DO NOT MODIFY                        *\n";
print OUTFILE " *                                                         *\n";
print OUTFILE " ***********************************************************\n";
print OUTFILE " */\n\n";


print OUTFILE "#include <emu/SysCallTableMacros.H>\n\n";

sub format($) {
    ($_[0] eq "") and return "\"0x%lx 0x%lx 0x%lx 0x%lx\"";
    ($_[0] eq "S_FMT") and return "\"%s 0x%lx 0x%lx 0x%lx\"";
    ($_[0] eq "SS_FMT") and return "\"%s %s 0x%lx 0x%lx\"";
    die $_[0] . " unsuported format request\n";
};

sub signextend($) {
    my $sl="";
    my @var=("a", "b", "c", "d", "e", "f");
    for my $f (split //,$_[0]) {
	my $v=shift @var;
	if ($f eq "1") { 
	    $sl=$sl . $v . " = SIGN_EXT(" . $v . "),";
	};
    };
    return $sl eq "" ? $sl:"(". substr($sl,0,-1) .")";
};

foreach my $row (@table) {
    if ($$row{"64"} ne "NYI") {
	if (substr($$row{"64"},0,3) eq "NYS") { 
	    # unimplemented but without error
	    condPrint 'SYSCALL_NYS('.$$row{"linux"}.",".
		&format($$row{"format"}).",".
	        $$row{"format"}.",".
		    ",," . substr($$row{"64"},3,length($$row{"64"})-3) . ");\n";
	} else {
	    #64 bit syscall and trace stub
	    condPrint "SYSCALL_DECLARE(".$$row{"64"}.",".
		&format($$row{"format"}).",".
	        $$row{"format"}.",".
		   ",);\n";
	}
    } else {
	condPrint 'SYSCALL_NYI('.$$row{"linux"}.",".
	    &format($$row{"format"}).
		",,);\n";
    }
    if($$row{"32"} ne "NYI") {
	if (substr($$row{"64"},0,3) eq "NYS") { 
	    condPrint 'SYSCALL_NYS('.$$row{"linux"}.",".
		&format($$row{"format"}).",".
	        $$row{"format"}.",".
		    ",," . substr($$row{"32"},3,length($$row{"32"})-3) . ");\n";
	} else {
	    condPrint 'SYSCALL_DECLARE('.$$row{"32"}.",".
		&format($$row{"format"}).",".
	        $$row{"format"}.",".
		    &signextend($$row{"signextend"}).",".
			$$row{"signextend"}.");\n";
	    if ($$row{"signextend"} ne "") {
		# a sign extend stub is needed
		condPrint 'SYSCALL_SIGNEXTEND('.$$row{"32"}.",".
		    &signextend($$row{"signextend"}).");\n";
	    }
	}
    } else {
	condPrint 'SYSCALL_NYI('.$$row{"linux"}.",".
	    &format($$row{"format"}). ",".
	    &signextend($$row{"signextend"}).",".
		$$row{"signextend"}.");\n";
    }
}

print OUTFILE 
    "#include <emu/sandbox.H>\t// for SyscallHandler typedef\n";

print OUTFILE "static SyscallHandler Syscall64[] = {\n";
my $comma="";
foreach my $row (@table) {
    print OUTFILE $comma;$comma=",\n";
    if ($$row{"64"} eq "NYI") {
	print OUTFILE "__NYI_" . $$row{"linux"};
    } elsif (substr($$row{"64"},0,3) eq "NYS") { 
	print OUTFILE "__trc_NYS_" . $$row{"linux"};
    } else {
	print OUTFILE "__trc_k42_linux_" . $$row{"64"}
    }
}
print OUTFILE "};\n\n";

$comma="";
print OUTFILE "static SyscallHandler SyscallTraced64[] = {\n";
foreach my $row (@table) {
    print OUTFILE $comma;$comma=",\n";
    if ($$row{"64"} eq "NYI") {
	print OUTFILE "__NYI_" . $$row{"linux"};
    } elsif (substr($$row{"64"},0,3) eq "NYS") { 
	print OUTFILE "__NYStraced_" . $$row{"linux"};
    } else {
	print OUTFILE "__traced_" . $$row{"64"};
    }
}
print OUTFILE "};\n\n";

$comma="";
print OUTFILE "static SyscallHandler Syscall32[] = {\n";
foreach my $row (@table) {
    print OUTFILE $comma;$comma=",\n";
    if ($$row{"32"} eq "NYI") {
	print OUTFILE "__NYI_" . $$row{"linux"};
    } elsif (substr($$row{"32"},0,3) eq "NYS") { 
	print OUTFILE "__trc_NYS_" . $$row{"linux"};
    } elsif ($$row{"signextend"} ne "") {
	print OUTFILE "__trc_signextend_" . $$row{"32"};
    } else {
	print OUTFILE "__trc_k42_linux_" . $$row{"32"};
    }
}
print OUTFILE "};\n\n";

$comma="";
print OUTFILE "static SyscallHandler SyscallTraced32[] = {\n";
foreach my $row (@table) {
    print OUTFILE $comma;$comma=",\n";
    if ($$row{"32"} eq "NYI") {
	print OUTFILE "__NYI_" . $$row{"linux"};
    } elsif (substr($$row{"32"},0,3) eq "NYS") { 
	print OUTFILE "__NYStraced_" . $$row{"linux"};
    } else {
	print OUTFILE "__traced". $$row{"signextend"} . "_" . 
	    $$row{"32"};
    }
}
print OUTFILE "};\n\n";

foreach my $row (@table) {
    print OUTFILE "#if (__NR_" . $$row{"linux"} . " != " . $$row{"index"} .
	")\n";
    print OUTFILE "#error __NR_" . $$row{"linux"} . 
	" should be " . $$row{"index"} . "and is __NR_" . $$row{"linux"} ."\n";
    print OUTFILE "#endif\n";
}
