# ############################################################################
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: ksitevalues.sh,v 1.3 2005/02/09 23:56:38 mostrows Exp $
# ############################################################################


# K42_SITE:	A simple identifier for your site.


K42_TOOLSDIR=`dirname $0`
HOSTNAME="hostname -f"
uname -a |grep -q AIX && HOSTNAME="hostname"
uname -a |grep -q Darwin && HOSTNAME="hostname"
: ${K42_SITE:="`$HOSTNAME`"}

case $K42_SITE in
    *watson*)
	K42_SITE=watson;
    ;;
   *torolab*)
	K42_SITE=torolab;
    ;;
    *toronto*)
	K42_SITE=toronto;
    ;;
    *arlx003*)
	K42_SITE=arl_austin;
    ;;
    *arl*)
    # This appears to not be in use
    #	: ${K42_PKGVER:=2}
    #	: ${K42_PKGHOST:=9.3.61.9}
    #	: ${K42_PACKAGES:=/nas/projects/k42/k42-packages}
    #	: ${K42_IMGLOC:=none}
	K42_SITE=arl;
    ;;
    bluewookie.austin.ibm.com)
	K42_SITE=bluewookie_austin;
    ;;
    *lanl*)
	K42_SITE=lanl
    ;;
    *ozlabs*)
	K42_SITE=ozlabs
    ;;
esac

for i in K42_SITE ; do 
    eval "echo $i=$`eval echo $i`" ; 
done
