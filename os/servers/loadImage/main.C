/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Utility program to download a new kernel image for
 *                     fast reboot.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <string.h>    // for strcmp
#include "loadImage.H"
#include <sys/systemAccess.H>

int
main(int argc, char *argv[])
{
    NativeProcess();

    if ((argc > 2) || ((argc == 2) && (strcmp(argv[1], "--help") == 0))) {
	fprintf(stderr, "Usage: %s [<boot_image>]\n", argv[0]);
	return 1;
    }

    const char *image = (argc == 2) ? argv[1] : "/knfs/boot/boot_image";
    return loadImage(argv[0], image);
}
