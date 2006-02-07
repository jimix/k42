#!/usr/bin/perl
#
# Make sure we are run by perl
#
#exec perl -S -x -- "$0" "$@"
#! perl
#
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: dezig.sh,v 1.13 2003/07/11 13:38:26 mostrows Exp $
#
#use strict;
use FileHandle;

require 'open3.pl';

sub fhbits {
  local(@fhlist) = split(' ',$_[0]);
  local($bits);
  $bits="";
  for (@fhlist) {
    vec($bits,fileno($_),1) = 1;
  }
  $bits;
}

sub usage{
  print "dezig - de-ziggify stack trace\n";
  print "Usage: dezig <file>\n";
  print "       <file> binary file with debugging symbols for gdb\n";
  print "       Standard input contains addresses (hex) to lookup\n";
}


format STDOUT =
    @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @>>>>>>>>>>>>>>>>>>>>>> @>>>
    $func,                                        $file,                 $line
.



my @symt = ();
$printfile = 1;
$addr2line = "powerpc64-linux-addr2line";
$noDeb = 0;
for ($x=0; $x<@ARGV; ++$x){
  if(@ARGV[$x] eq "-n"){
    $noDeb = 1;
  }elsif(@ARGV[$x] eq "-f"){
    $printfile = 0;
  }else{

    my $binary = "";
    if(-e @ARGV[$x]){
      $binary = @ARGV[$x];
    }else{
      print "Searching for possible matches...\n";
      $findexp = @ARGV[$x];
      open FIND, "find . -name '*$findexp*'|";
      $num = 1;
      my @files;
      while(<FIND>){
	print "$num: $_";
	@files[$num] = $_;
	++$num;
      }
      if($num > 2){
	print "Choose a binary image: ";
      INPUT: while(<STDIN>){
	  last INPUT if($_ < $num  && $num > 0);
	}
	$binary = @files[$_];
      }else{
	$binary = @files[1];
      }
    }
    print "Using binary image: $binary\n";

    my $fileData = {};
    $fileData->{bin} = $binary;

    open3($fileData->{ToGDB}, $fileData->{FromGDB}, $fileData->{ErrGDB},
	  $addr2line . " -C -s -f -e " . $binary);

    $fileData->{FromGDB}->autoflush(1);
    $fileData->{ToGDB}->autoflush(1);

##    open3(T, F , E, $addr2line . " -C -s -f -e " . $binary);
#    $fileData->{ToGDB} = *T;
#    $fileData->{FromGDB} = *F;
#    $fileData->{ErrGDB} = *E,


    push @symt, $fileData;
#    
#    print "File does not exist: " . @ARGV[$x] . "\n";
#    usage;
#    exit;
  }
}

#if($binary eq ""){
#  usage;
#  exit;
#}


#open3(ToGDB,FromGDB,ErrGDB, $addr2line . " -C -s -f -e " . $binary);


$state = 0;

while($inputLine =<STDIN>){
  if($inputLine=~/call chain:(.*)$/){
    $inputLine = $1;
  }

  if($inputLine=~/^([ ]*((([0-9a-f]{8}){1,2})))+/){
#    print "Input line: $inputLine\n";
    my @words = split(/ /, $inputLine);
#    print "Words: " . join(":",@words). "\n";
    foreach $a (@words) {
      if($a =~ /([0-9a-f]{8,16})/){
#	print "Symbol: $a\n";
	my $i = 0;
	my $val = $1;
      SYMLOOKUP:while($i <=$#symt) {
	  my $f = $symt[$i];
	  $i++;
	  print {$f->{ToGDB}} "0x" . $val . "\n";
	  my $from = $f->{FromGDB};
	  $func = <$from>;
	  $location = <$from>;
	  if($func=~/^\?\?.*/ && $i<=$#symt) {
	  }else{
	    ($file,$line) = split(/:/,$location);
	    write;
	    last SYMLOOKUP;
	  }
	}
      }
    }
  }else{
    $inputLine =~/^(.*)$/;
    print "$1\n";
  }
}


