#ifndef __LOAD_IMAGE_H_
#define __LOAD_IMAGE_H_
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

int loadImage(const char *prog, const char *image);

#endif /* #ifndef __LOAD_IMAGE_H_ */
