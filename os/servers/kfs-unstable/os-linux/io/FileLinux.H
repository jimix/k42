#ifndef __FILE_LINUX_H_
#define __FILE_LINUX_H_

#ifdef KFS_TOOLS
#define _LARGEFILE64_SOURCE
#include <sys/stat.h>
#else
#include <linux/stat.h>
#endif

#include <fslib/direntk42.H>

class FileLinux {
public:
    /*
     * This is an experiment, do we want to encapsulate standard Unix
     * structs in classes that make them easier to manipulate???
     */
    typedef struct stat64 Stat;
};



#endif /* #ifndef __FILE_LINUX_H_ */
