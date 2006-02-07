/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libksup.C,v 1.1 2001/06/12 21:53:37 peterson Exp $
 *************************************************************************** */

void
localInitPutChar(char c)
{
    // Due to problems with GUD (gdb under emacs) we strip \r
    if (c == '\r') return;

    uval8 uc;
    uc = (uval8) c;

    // FIXME NEED_HW_SOLUTION
}


