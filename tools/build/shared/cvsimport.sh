#!/bin/ksh

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: cvsimport.sh,v 1.8 2000/12/12 23:18:21 dilma Exp $
# ############################################################################

#set -x 

# defaults
export DEFAULTCVSROOT=${DEFAULTCVSROOT:-/stumm/d0/tornado/torncvs}
# consider changing to
#export CVS_RSH=ssh
#export DEFAULTCVSROOT=${DEFAULTCVSROOT:-:ext:${USER}@halfdome.eecg:/stumm/d0/tornado/torncvs}
export DEFAULTCVSMODULE=${DEFAULTCVSMODULE:-kitchsrc}
export DEFAULTCVSSPOOLDIR=${DEFAULTCVSSPOOLDIR:-~kitchawa}
export DEFAULTCVSSPOOLID=${DEFAULTCVSSPOOLID:-kitchsrc_nightly_}
#export DEFAULTCVSCURRENTDIRFILTER=${DEFAULTCVSCURRENTDIRFILTER:-\
#'(Make.objinfo)|(Make.config)|(.*~$)|(#.*#$)|(/\.#)'}
export DEFAULTCVSVENDORTAG="WATSON"

# Working variables
export STARTDIR=$(pwd)
export TMPDIR=${TMPDIR:-/tmp}
export EDITOR=${EDITOR:-vi}
export PAGER=${PAGER:-less}
export WORKINGDIR=$TMPDIR/cvsimport.$$
export CVSCMD=${CVSCMD:-cvs}
export CVSROOT=$DEFAULTCVSROOT
export CVSMODULE=$DEFAULTCVSMODULE
export SPOOLDIR=$DEFAULTCVSSPOOLDIR
export SPOOLID=$DEFAULTCVSSPOOLID
export SPOOLDATE=''
export VENDORTAG="$DEFAULTCVSVENDORTAG"
#export CURRENTDIRFILTER="$DEFAULTCVSCURRENTDIRFILTER"
#export CURRENTFILEFILTER="$CURRENTDIRFILTER"
export IGNORELIST="ignorelist.cvsimport"
 
# WORKING FILE NAMES
export NEWDIRS="NEW-DIRS"
export CURRENTDIRS="CURRENT-DIRS"
export NEWFILES="NEW-FILES"
export CURRENTFILES="CURRENT-FILES"
export DIFF="DIFF-DIRS"
export DIRSNOTINCURRENT="NOTINCURRENT-DIRS"
export DIRSNOTINNEW="NOTINNEW-DIRS"
export FILESNOTINCURRENT="NOTINCURRENT-FILES"
export FILESNOTINNEW="NOTINNEW-FILES"
export CHECKOUTLOG="checkout.log"
export UPDATELOG="update.log"
export IMPORTLOG="import.log"
export COMMITLOG="commit.log"
export IMPORTCONFLICTS="import.conflicts"

mycleanup () {
   print -u2 "Exiting via mycleanup!"
    cd $STARTDIR
    if [[ -z $NORM ]]
    then
      if [[ -d $WORKINGDIR ]]
      then
        print -u2 "Cleaning $WORKINGDIR ..."
        print -u2 "!!! For now, you must do this manually using the following command: !!!"
        echo "/bin/rm -rf $WORKINGDIR"
      fi
    fi
    exit 1
}

mkworkingdir () {
  #set -x
  print -u2 "Preparing $WORKINGDIR ..."
  if [[ -d $WORKINGDIR ]]
  then
     print -u2 "Cleaning $WORKINGDIR ..." 
     rm -rf $WORKINGDIR
  fi

  if ! mkdir $WORKINGDIR
  then
    print -u2 "ERROR: unable to create $WORKINGDIR"
    mycleanup
  fi

  if ! cd $WORKINGDIR
  then
    print -u2 "ERROR: unable to set current directory to $WORKINGDIR"
    mycleanup
  fi
 
  if ! mkdir new
  then
    print -u2 "ERROR: unable to create $WORKINGDIR/new"
    mycleanup
  fi

  if ! mkdir current
  then
    print -u2 "ERROR: unable to create $WORKINGDIR/current"
    mycleanup
  fi
}

unspool_newversion () {
  #set -x
  # untar spooled copy of new in new
  print -u2 "Unspooling new version in $WORKINGDIR/new ..."
  if ! cd $WORKINGDIR/new
  then
    print -u2 "ERROR: unable to set current directory to $WORKINGDIR/new"
    mycleanup
  fi

  ls $SPOOLDIR/$SPOOLID*.tar.gz | read SPOOLFILE
  SPOOLDATE=${SPOOLFILE##*$SPOOLID}
  SPOOLDATE=${SPOOLDATE%%.tar.gz}
  SPOOLID=$SPOOLID$SPOOLDATE

  if ! gunzip -d -c $SPOOLFILE | tar -xf -
  then
    print -u2 "ERROR: unable to set current directory to $WORKINGDIR/new"
    mycleanup
  fi 

  if [[ ! -d $CVSMODULE ]]
  then
    print -u2 "ERROR: unable to find $CVSMODULE in new version $WORKINGDIR/new\
from $SPOOLFILE"
    mycleanup
  fi
  cd $WORKINGDIR
}

checkout_current () {
  #set -x
  # checkout a freshcopy in current
  print -u2 "Checking out a copy of the current version in $WORKINGDIR/current\
 ..."

  if ! cd $WORKINGDIR/current
  then
    print -u2 "ERROR: unable to set current directory to $WORKINGDIR/current"
    mycleanup
  fi
  
  if ! $CVSCMD checkout -kk $CVSMODULE > $WORKINGDIR/$CHECKOUTLOG 2>&1
  then
    print -u2 "ERROR: checkout of $CVSMODULE failed \(CVSROOT=$CVSROOT\)"
    mycleanup
  fi
  cd $WORKINGDIR
}

cvsdirs () {
  #set -x
  # Create list of directories and files in the new and current version   
  typeset current="$WORKINGDIR/current/$CVSMODULE"
  typeset new="$WORKINGDIR/new/$CVSMODULE"

  print -u2 "Creating list of dirs in new version \($new\) ..."
  
  if ! cd $new
  then	
    print -u2 "ERROR: unable to find new version $new"
    mycleanup
  fi
  
  if ! find . -type d  -print | sort > $WORKINGDIR/$NEWDIRS
  then
    print -u2 "ERROR: in creating list of directories from new version \($new\)"
    mycleanup
  fi

  if ! find . -type f -print | sort > $WORKINGDIR/$NEWFILES
  then
    print -u2 "ERROR: in creating list of files from new version \($new\)"
    mycleanup
  fi
  print -u2 "Creating list of dirs in current version \($current\) ..."

  if ! cd $current
  then	
    print -u2 "ERROR: unable to find current checkout version $current"
    mycleanup
  fi

#  if ! find . -type d  -print | egrep -v "$CURRENTDIRFILTER" | \
#	egrep -v '/mips64' | sort  > $WORKINGDIR/$CURRENTDIRS
  if ! find . -type d -print | sort > $WORKINGDIR/$CURRENTDIRS
  then
    print -u2 "ERROR: in creating list of directories from current version /
\($current\)"
    mycleanup
  fi

#  if ! find . -type f  -print | egrep -v "$CURRENTFILEFILTER" | \
#	egrep -v '/mips64' | sort  > $WORKINGDIR/$CURRENTFILES
  if ! find . -type f -print | sort > $WORKINGDIR/$CURRENTFILES
  then
    print -u2 "ERROR: in creating list of files from current version\
 \($current\)"
    mycleanup
  fi
  cd $WORKINGDIR
}

diffdirs () {
 #set -x
  # Create list of directories exclusive to either new or current versions
  print -u2 "Creating lists of differences ..."

  if ! comm -23 $CURRENTDIRS $NEWDIRS > $DIRSNOTINNEW 
  then
    print -u2 "ERROR: Could not create list of Directories exclusively in the\
 current version"
    mycleanup
  fi

  if ! comm -13 $CURRENTDIRS $NEWDIRS > $DIRSNOTINCURRENT
  then
    print -u2 "ERROR: Could not create list of Files exclusively in the current\
 version"
    mycleanup
  fi
  
  if ! comm -23 $CURRENTFILES $NEWFILES > $FILESNOTINNEW
  then
    print -u2 "ERROR: Could not create list of Directories exclusively in the\
 new version"
    mycleanup
  fi
  
  if ! comm -13 $CURRENTFILES $NEWFILES > $FILESNOTINCURRENT
  then
    print -u2 "ERROR: Could not create list of Files exclusively in the new\
 version"
    mycleanup
  fi

}

ignore () {
  #set -x
  target="$1"
  cat $WORKINGDIR/current/$CVSMODULE/$IGNORELIST | while read pat
  do
     if [[ "$target" = $pat ]]
     then
        return 0
     fi
  done
  return 1
}

process_dir () {
  #set -x
  typeset dir=$2
  typeset base=$1
  typeset version=$3
  typeset choice=''
  typeset -i rtn


  if [[ -a $base/$dir ]]
  then
    print "DIR: $dir in $version version" 
    select choice in \
       'delete'\
       'rename'\
       'ignore'\
       'Add to ignore list'\
       'exit'
    do
       case $choice in 
         'delete' ) echo "$REPLY: delete not implemented" ;;
         'rename' ) echo "$REPLY: rename not implemented" ;;
         'ignore' ) echo "$REPLY: ignore not implemented"; break ;;
         'Add to ignore list' ) echo "$REPLY: ignore not implemented"; break ;;
         'exit'   ) rtn=1;break;;
          *       ) echo "HUH: $REPLY: $choice"
                    print "DIR: $dir in $version version"  ;;
       esac
    done
  fi
  return $rtn
}

process_file () {
  #set -x
  typeset file=$2
  typeset base=$1
  typeset version=$3
  typeset choice=''
  typeset comment="Removed as per Merge of $SPOOLID$SPOOLDATE"
  typeset -i rtn=0

  if ignore $file
  then
    return
  fi

  if [[ -a $base/$file ]]
  then
    print "FILE: $file in $version version"
    select choice in \
      'delete'\
      'rename'\
      'edit'\
      'ignore'\
      'add file to ignore list'\
      'add path to ignore list'\
      'exit'
    do
       case $choice in 
         'delete' )
                 rm $base/$file
                 $CVSCMD remove $base/$file
                 $CVSCMD commit -m "$comment" $base/$file
                 break;;
         'rename' ) echo "$REPLY: rename not implemented" ;;
         'edit'   ) 
                 $EDITOR $base/$file;;
         'ignore' )
                  break ;;
         'add file to ignore list' )
	          echo "*/${file##*/}" >> \
                        $WORKINGDIR/current/$CVSMODULE/$IGNORELIST
                  break ;;
         'add path to ignore list' )
	          echo $file >> $WORKINGDIR/current/$CVSMODULE/$IGNORELIST
                  break ;;
         'exit'   ) rtn=1; break;;
          *       ) echo "HUH: $REPLY: $choice"
                    print "FILE: $file in $version version" ;;
       esac
    done
  fi
  return $rtn
}

process_notinnew () {
  #set -x
  typeset oldprompt="$PS3"
  typeset dir=''
  typeset file=''

  PS3="Deleted remote or Added locally ?>"

  # process dirs which are only in current version
  print -u2 "Processing Directories in current but not in new..."
  while read -u3 dir
  do
    # filter through ignore list
    if ! ignore $dir
    then
      if ! process_dir $WORKINGDIR/current/$CVSMODULE $dir 'current'
      then
        break
      fi
    fi
  done 3< $DIRSNOTINNEW

  # process files which are only in current version
  print -u2 "Processing Files in current but not in new..."
  while read -u4 file
  do
    #filter through ignore list
    if ! ignore $file
    then
      if ! process_file $WORKINGDIR/current/$CVSMODULE $file 'current'
      then
        break
      fi
    fi
  done 4< $FILESNOTINNEW

  PS3="$oldprompt"
}

process_notincurrent () {
  #set -x
#for each file
# 1) if added by then and we want to reflect the add then
#    do nothing (merge handles this case (confirm for both dirs and files)
# or 2) if deleted by us and we don't want it readded then dosomething???
#  #set -x
  typeset oldprompt="$PS3"
  typeset dir=''
  typeset file=''

  PS3="Added remote or Deleted locally ?>"

  # process dirs which are only in new version
  print -u2 "Processing Directories in new but not in current..."
  while read -u3 dir
  do
    if ! process_dir $WORKINGDIR/new/$CVSMODULE $dir 'new'
    then
      break
    fi
  done 3< $DIRSNOTINCURRENT

  # process files which are only in current version
  print -u2 "Processing Files in new but not in current..."
  while read -u4 file
  do
    if ! process_file $WORKINGDIR/new/$CVSMODULE $file 'new'
    then
      break
    fi
  done 4< $FILESNOTINCURRENT

  PS3="$oldprompt"
}

viewmenu () {
  #set -x
  typeset choice
  typeset oldprompt="$PS3"
  PS3="$PS3:view:"
  select choice in \
    'Dirs in current but not in new' \
    'Files in current but not in new' \
    'Dirs in new but not in current' \
    'Files in new but not in current' \
    'exit'
  do
    case $choice in
      'Dirs in current but not in new'  ) $PAGER $WORKINGDIR/$DIRSNOTINNEW;;
      'Files in current but not in new' ) $PAGER $WORKINGDIR/$FILESNOTINNEW;;
      'Dirs in new but not in current'  ) $PAGER $WORKINGDIR/$DIRSNOTINCURRENT;;
      'Files in new but not in current' ) $PAGER $WORKINGDIR/$FILESNOTINCURRENT;;
      'exit'                            ) break;;
    esac
  done
  PS3="$oldprompt"
}

diffmenu () {
  #set -x
  typeset choice
  typeset oldprompt="$PS3"
  PS3='diffs:'
  select choice in \
     'view'    \
     'process in current but not in new' \
     'process in new but not in current' \
     'save ignore list' \
     'preserve tmp dir' \
     'continue' \
     'quit' 
  do
    case $choice in
      'view'                  ) viewmenu;;
      'process in current but not in new'    ) process_notinnew;;
      'process in new but not in current') process_notincurrent;;
      'save ignore list'      ) $CVSCMD commit -m "Saving cvsimport\
 ignorelist" $WORKINGDIR/current/$CVSMODULE/$IGNORELIST ;;
      'preserve tmp dir'   ) export NORM=1;;
      'continue'              ) break;;
      'quit'                  ) mycleanup;;
    esac
  done
  PS3="$oldprompt"
}

do_import () {
  #set -x
  typeset releasetag="${VENDORTAG}_${SPOOLDATE}"
  print -u2 "Doing Import ..."
  cd $WORKINGDIR/new/$CVSMODULE
  $CVSCMD import -I '!' -I CVS -m "CVSMERGE $SPOOLDATE" $CVSMODULE $VENDORTAG\
 $releasetag > $WORKINGDIR/$IMPORTLOG 2>&1
  echo "Check for errors HERE!"
  cd $WORKINGDIR
}

process_conflicts () {
  #set -x
  print -u2 "Processing Conficts ..."
  grep '^C' $WORKINGDIR/$IMPORTLOG > $WORKINGDIR/$IMPORTCONFLICTS
  cd $WORKINGDIR/current/
  while read -u5 type file
  do
    $CVSCMD update -kk -j $VENDORTAG:yesterday -j $VENDORTAG $file
    if [[ ! -a $file ]]
    then
      print -u2 "File not found while doing update (to handle conflict)\n"
      print -u2 "If file was 'cvs removed' and resurrected try:\n"
      print -u2 "    cp $WORKINGDIR/new/$file $WORKINGDIR/current/$file\n"
      print -u2 "    cvs add $WORKINGDIR/current/$file (may need to cd)\n"
      print -u2 "Do Control-Z now to fix things and hit return when done..."
      read
    elif grep -q '<<<<<<' $file; then
      print -u2 "Press return to edit..."
      read
      $EDITOR $WORKINGDIR/current/$file
    fi
    $CVSCMD commit -f -m "CVSIMPORT: merged changes" $file
  done 5< $WORKINGDIR/$IMPORTCONFLICTS
  cd $WORKINGDIR
}

do_update () {
    #set -x
    print -u2 "Doing update ..."
    cd $WORKINGDIR/current/$CVSMODULE
    $CVSCMD update -d >$WORKINGDIR/$UPDATELOG 2>&1
    echo "Check for errors HERE!"
    cd $WORKINGDIR
}

do_commit () {
  #set -x
  print -u2 "Doing commit ..."
  cd $WORKINGDIR/current/$CVSMODULE
  $CVSCMD  commit -m "CVSIMPORT: merged changes" \
 >$WORKINGDIR/$COMMITLOG 2>&1
  echo "Check for errors HERE!"
  cd $WORKINGDIR
}

usage () {
   print -u2 "USAGE: cvsimport spoolid"
}
#set cleanup code to run on abnormal exits
trap mycleanup HUP INT QUIT KILL ABRT TERM 


#process cmdline args and verify
origargs="$@"

integer optcount=0
while getopts "whi:s:d:" OPT
do
  case $OPT in
     ("h")   usage; exit;;
     ("i")   export SKIPINIT=1; 
             export WORKINGDIR="$OPTARG";
             (( optcount=optcount + 2 ));;
     ("s")   export SPOOLID="$OPTARG";
             (( optcount=optcount + 2 ));;
     ("d")   export SPOOLDATE="$OPTARG";
             (( optcount=optcount + 2 ));;
     ("w")   export NORM=1; 
             (( optcount=optcount + 1 ));;
  esac
done
shift $optcount

#init working directory unless specified not to
if [[ -z $SKIPINIT ]]
then
  mkworkingdir

  unspool_newversion

  checkout_current
else
  print -u2 "Assuming working directory already exists with correct data"
  if ! cd $WORKINGDIR
  then
    print -u2 "ERROR: Unable to cd to $WORKINGDIR\n"
    mycleanup
  fi
fi

# build list of files and dirs in new and current versions
cvsdirs

# create lists of differences
diffdirs

# process the differences
diffmenu

# do import (in new directory)
do_import

# process conflicts (in checked out copy)
process_conflicts

# do update (in checked out copy)
# do_update

# do commit (in checked out copy)
# do_commit

mycleanup

