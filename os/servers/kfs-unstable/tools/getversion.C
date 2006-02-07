/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getversion.C,v 1.1 2004/03/07 00:39:35 lbsoares Exp $
 *****************************************************************************/
#include "fs.H"
#include "SuperBlock.H"
#include <stdio.h>

int
main(int argc, char *argv[])
{
    printf("%d\n", SuperBlock::KFS_VERSION);
    return 0;
}
