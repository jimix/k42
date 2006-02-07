#ifndef __FIXUP_H_
#define __FIXUP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fixup.H,v 1.6 2004/02/27 17:14:31 mostrows Exp $
 *****************************************************************************/
/* A way to safely include some basic linux-kernel headers */
extern "C" {
#define _PPC_STRING_H_
#define eieio __linux__eieio
#define new __C__new
#define virtual __C__virtual
#define namespace __C__namespace
#define private __C__private

#include <linux/config.h>
#include <linux/linkage.h>
#include <linux/posix_types.h>
#include <linux/types.h>

#define __dev_t_defined
#define __gid_t_defined
#define __ino_t_defined
#define __mode_t_defined
#define __nlink_t_defined
#define __off_t_defined
#define __uid_t_defined
#define __blkcnt_t_defined
#define __blksize_t_defined

#include <sys/cdefs.h>
#undef __THROW
#define __THROW
#include <asm/string.h> // Need to force this to get the lk file because
//#include <string.h>     // this include does fancy footwork to get asm/string.h
#include <asm/param.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <string.h>
#include <linux/wait.h>
#include <linux/aio.h>
#undef new
#undef virtual
#undef namespace
#undef eieio
#undef private
//These would otherwise pollute the K42 name-space
#undef PAGE_SIZE
#undef PAGE_MASK
#undef SEGMENT_SIZE


}

#endif /* #ifndef __FIXUP_H_ */
