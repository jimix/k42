# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.18 2004/01/31 17:53:03 aabauman Exp $
# ############################################################################

default:: targets


TARGETS += cpp5.exe

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

HOST_CFLAGS = -g -MD
LEXLIB = -lfl

# FIXME: need to do this because the generated code has warnings
WARN_FLAGS = -Wall -Wno-implicit -Wno-unused -Wno-parentheses

# the yacc error is known so only report the error if it is different.
YACC_ERR=byacc: 290 shift/reduce conflicts, 100 reduce/reduce conflicts.

# this fancy rule is here because we need make to consider the
# exit status of $(YACC) NOT the sed, which is always true.
#
y.tab.c : cpp5.y
	(exec 3>&1; status=`(( \
	  $(YACC) -dl  $< \
	  2>&1 1>&3 3>&- 4>&-; $(ECHO) "$$?" >&4 ) | \
	  $(SED) -e '\?$(YACC_ERR)?d' 1>&2 3>&- 4>&-) 4>&1`; $(EXIT) $$status)

lex.yy.c : cpp5.l
	$(LEX)  $?

cpp5.exe : y.tab.o lex.yy.o
	$(HOST_CC) -o cpp5.exe $^ $(LEXLIB)

clean ::
	-$(RM) y.tab.o y.tab.c y.tab.h lex.yy.o lex.yy.c cpp5.exe *~
$(MKTOOLBIN)/stubgen:
	$(INSTALL) --mode 0775 -d $@

install_targets:: $(MKTOOLBIN)/stubgen $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $<
	$(RM) cpp5.exe *.o *.d

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
