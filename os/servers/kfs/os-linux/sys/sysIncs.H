#ifndef __SYS_INCS_H_
#define __SYS_INCS_H_

//FIXME g++ is current defining NULL to be (void *)0 
#undef NULL
#define NULL 0

#include <sys/BasicTypes.H>
#ifndef KFS_TOOLS
#include <sync/Lock.H>
#include <sys/SysStatus.H>
#endif
#include <sys/types.H>

#include <linux/limits.h>
#include <linux/string.h>

#endif /* #ifndef __SYS_INCS_H_ */
