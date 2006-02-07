# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.1 2005/06/17 21:19:27 jappavoo Exp $
# ############################################################################

EXTRA_INCDIRS = -I/u/kitchawa/local/tcl8.4/include/ -I/u/bseshas/readline/aix/include -I/u/bseshas/tcl/include/readline
EXTRA_LIBDIRS = -L/u/bseshas/readline/aix/lib -L/usr/X11R6/lib -L/u/kitchawa/local/tcl8.4/lib
EXTRA_LIBS = -lpthread -ldl -lm -ltcl8.4 -lhistory -lreadline -ltermcap
 
all: kore

kore.o : kore.c
	gcc $(EXTRA_INCDIRS) -c kore.c
kore: kore.o
	gcc $(EXTRA_LIBDIRS) $(EXTRA_LIBS) kore.o -o kore