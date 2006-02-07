/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: longexec.C,v 1.3 2005/06/28 19:42:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/systemAccess.H>

#include <stdio.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    NativeProcess();

    /*
     * Because we're a mixed-mode program, the execv() below will automatically
     * be a "long" exec.
     */

    execv(argv[1], &(argv[1]));
}
