# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: reboot.sh,v 1.2 2004/02/18 18:44:41 butrico Exp $
# ############################################################################

(echo 0; echo X$1) > /ksys/console
