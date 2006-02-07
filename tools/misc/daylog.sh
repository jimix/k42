#!/bin/sh
#
# Make sure we are run by perl
#
exec perl -S -x -- "$0" "$@"
#! perl
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000,2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: daylog.sh,v 1.12 2004/06/11 14:33:42 andrewb Exp $
# ############################################################################
#
#****************DO NOT CHANGE THIS IN /u/kitchawa/bin***********************
#****************UPDATE THE COPY IN tools/misc in the tree*******************
#****************AND COPY TO /u/kitchawa/bin*********************************

my $cvsroot = $ENV{'CVSROOT'};
open(LOG,"<" . $cvsroot  . "/CVSROOT/commitlog.today");
my $inparagraph=0;
my $blank=0;
my $hide = 0;
my $output = "";
my $matched=0;
my $empty=1;
LINE: while(<LOG>){
  if(/^\*+$/){
    if( $inparagraph!=0 ){
      if($matched>0){
	if($output ne ""){
	  print $output;
	  $empty=0;
	}
	print "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
      }
      $output = "";
      $inparagraph=0;
    }
    next LINE;
  }

  my $line=$_;
  if(/^$/){
    $blank = 1;
    next LINE;
  }elsif($blank!=0 && $inparagraph!=0){
    $blank=0;
  }
  split;

  if(@_[0] eq "Date:"){
    $date = @_[6];
  }
  if(@_[0] eq "Author:"){
    $output = $output . @_[0] . "\t" . @_[1] . " @ " . $date . "\n";
  }

  if(@_[0] eq "Update"){
    $dir = @_[2];
    $dir =~ s/^.*kitchawa\/cvsroot\///;
    $cvsmodule = (split  '/', $dir)[0];
    # No arg -> match everybody, else look for a match
    if(@ARGV==0){
      $matched = 1;
    }else{
      $matched = 0;
      foreach $module (@ARGV){
	if($module eq $cvsmodule){
	  ++$matched;
	}
      }
    }
  }
  if(@_[1] eq "Files:"){
    $inparagraph=1;
    $output = $output . @_[0] . " [..." . $dir . "]\n";
    next LINE;
  }
  if(@_[1] eq "Message:"){
    $inparagraph = 1;
  }

  if($inparagraph){
    $output = $output . $line ;
  }
}
if($matched){
  print $output;
  $empty=0;
}
exit $empty;
